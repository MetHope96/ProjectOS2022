#include <proc_syscall.h>
#include <proc.h>
#include <addrspace.h>
#include <syscall.h>
#include <kern/errno.h>
#include <synch.h>
#include <current.h>
#include <thread.h>
#include <copyinout.h>
#include <kern/wait.h>
#include <vfs.h>
#include <copyinout.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vm.h>
#include <lib.h>


int sys_getpid(pid_t *curproc_pid) {
	*curproc_pid = curproc->proc_id; //Return the pid
	return 0;
}

int sys_fork(pid_t *child_pid, struct trapframe *tf){

  struct trapframe *tf_child; //Stack of the exception handled
  struct proc *child_proc=NULL;
  int err;

  if (proc_counter >= MAX_PROC){
    return ENPROC; //There are too many process on the system.
  }

  child_proc = proc_create("child_proc");//return a new process

  if(child_proc == NULL){
    return ENOMEM; //Sufficient virtual memory for the new process was not available.
  }

  child_proc->parent_id = curproc->proc_id; //Update parent process

    spinlock_acquire(&curproc->p_lock);
      if (curproc->p_cwd != NULL) {
            VOP_INCREF(curproc->p_cwd);
            child_proc->p_cwd = curproc->p_cwd; //Copy the current directory
      }
    spinlock_release(&curproc->p_lock);

    as_copy(proc_getas(), &(child_proc->p_addrspace)); //Address space copy
    if(child_proc->p_addrspace == NULL){
    proc_destroy(child_proc); 
    return ENOMEM; 
    }

	tf_child = kmalloc(sizeof(struct trapframe));
	if(tf_child == NULL){
    return ENOMEM; //Sufficient virtual memory for the new process was not available.
    }

	*tf_child = *tf; //child trapframe = parent trapframe 

  /*Copy of open elements file table: */
  for(int i = 0; i < OPEN_MAX; i++){
    if(curproc->file_table[i] != NULL){
      lock_acquire(curproc->file_table[i]->lock);//Lock the filetablei[i]
	  curproc->file_table[i]->ref_count ++;
      child_proc->file_table[i] = curproc->file_table[i];
      lock_release(curproc->file_table[i]->lock);//Release the lock at filetable[i]
    }
  }

    // Launch the child execution (first thread creation)
    err = thread_fork("child",child_proc,(void*)enter_forked_process,
                      tf_child,(unsigned long)child_proc->p_addrspace);
    if(err){
        return err;
    }

/*Set the return value as a child_pid */
  *child_pid = child_proc->proc_id;
  return 0;
}

void sys_exit(int exitcode){
	int i = 0;
//Find the index of curproc in the proc table
	for(i = 0; i < MAX_PROC; i++){
		if(proc_table[i] != NULL){
			if(proc_table[i]->proc_id == curproc->proc_id){
			break;
			}
		}
		if(i == MAX_PROC - 1)
		panic("Current process not found in process table");
	}


 	lock_acquire(curproc->lock);
	curproc->exit_status = 1;
	curproc->exit_code = exitcode;
	KASSERT(curproc->exit_status == proc_table[i]->exit_status);
	KASSERT(curproc->exit_code == proc_table[i]->exit_code);
	cv_signal(curproc->cv, curproc->lock);// Wake up one thread that's sleeping on this CV.
	lock_release(curproc->lock);


	thread_exit();
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t* retval) {

	int i = 0;
	if(options != 0){
		return EINVAL;
	}
	if(curproc->proc_id == pid){
		return ECHILD;
	}
	//Check pid is present in proc_table
	for(i = 0; i < MAX_PROC; i++){
		if(proc_table[i] != NULL){
			if(proc_table[i]->proc_id == pid)
			break;
		}
		if(i == MAX_PROC - 1)
		return ESRCH;
	}

	if(proc_table[i]->parent_id != curproc->proc_id){
		return ECHILD;
	}
	
	KASSERT(proc_table[i] != NULL);
	lock_acquire(proc_table[i]->lock);

	cv_wait(proc_table[i]->cv, proc_table[i]->lock); //Release the supplied lock, go to sleep, and, after waking up again, re-acquire the lock.

	lock_release(proc_table[i]->lock);
	*retval = proc_table[i]->proc_id;

    *status = proc_table[i]->exit_code;

	proc_destroy(proc_table[i]);

	return 0;
}

int sys_execv(char *program, char **args){

    int err, argc=0, pad=0;
    int args_size=0; // Total size of kernel buffer (args size + pointers size)
    struct vnode *vn;
    struct addrspace *as;
    vaddr_t entrypoint, stackptr;
    size_t arglen=0;
    char **kargs; // Kernel buffer
    char *kargs_ptr; // To move inside kernel buffer (char by char)
    char *kargs_ptr_start; // Starting position of arguments inside kernel buffer

    // Check arguments validity
    if((program == NULL) || (args == NULL)) {
		return EFAULT;
	}

    /* 1. Compute argc */

    // The n. of elements of args[] is unknown but the last argument should be NULL.
    // Compute argc = n. of valid arguments in args[]
    while(args[argc] != NULL){
        // Accumulate the size of each args[] element
        arglen = strlen(args[argc])+1;
        args_size += sizeof(char)*(arglen);
        // Compute n. of 0s for padding (if needed)
        if((arglen%4) != 0){
            pad = 4 - arglen%4;
            args_size += pad;
        }
        // The total size of the argument strings exceeeds ARG_MAX.
        if(args_size > ARG_MAX){
            err = E2BIG;
            return err;
        }
        argc ++;
    }
    // Now argc contains the number of valid arguments inside args[]
    // and arg_size contains the total size of all arguments (also considering padding zeros)

    /* 2. Copy arguments from user space into a kernel buffer. */

    // - Program path
    char *kprogram = kstrdup(program);
    if(kprogram==NULL){
        return ENOMEM;
    }

    // - Single arguments with their pointers

    // Update the tot size to can insert the pointers
    args_size = args_size + sizeof(char *)*(argc+1);

    // Allocate the kernel buffer (choose a pointer inside kernel heap)
    kargs = (char **)kmalloc(args_size);

    // The starting position is over the arg pointers
    kargs_ptr_start = (char *)(kargs + argc + 1); // e.g. A + 16

    // Initialize the pointer to move inside kernel buffer
    kargs_ptr = kargs_ptr_start;

    // Fill the kernel buffer
    for(int i=0;i<argc;i++){ // e.g. the args are "foo\0" "hello!\0" "1\0"
        
        // - Copy the arguments pointers into kernel buffer
        kargs[i] = kargs_ptr;

        // - Copy the arguments into kernel buffer
        err = copyinstr((const_userptr_t)args[i], kargs_ptr, ARG_MAX, &arglen); // e.g. arglen = 4, 7, 2
        if(err){
            return err;
        }

        // Move pointer on the copied argument
        kargs_ptr += arglen;

        // If the argument string has no exactly 4 bytes 0-padding is needed
        if((arglen%4) != 0){
            // n. of bytes to pad
            pad = 4 - arglen%4;
            for(int j=0;j<pad;j++){
                // Move on the pointer by following the 0s added
                kargs_ptr += j; 
                // Add the \0
                memcpy(kargs_ptr,"\0",1);
            }
        }
    }
    // Now kargs[] is the copy of args in kernel space with the proper padding
    // and kargs_ptr points to the last char of kernel buffer

    // Set the last pointer to null
    kargs[argc] = NULL;

    /* 3. Create a new address space and load the executable into it
     * (same of runprogram) */

    // Open the "program" file
    err = vfs_open(program, O_RDONLY, 0, &vn);
    if(err){
        return err;
    }

    // Create a new address space
	as = as_create();
	if(as == NULL) {
		vfs_close(vn);
		err = ENOMEM;
        return err;
	}

    // Change the current address space and activate it
	proc_setas(as);
	as_activate();

    // Load the executable "program"
	err = load_elf(vn, &entrypoint);
	if (err) {
		vfs_close(vn);
		return err;
	}

    // File is loaded and can be closed
	vfs_close(vn);

    // Define the user stack in the address space
	err = as_define_stack(as, &stackptr);
	if (err) {
		return err;
	}

    /* 4. Copy the kernel buffer into stack (remember to update the pointers) */

    // Start position of the stack (= allocate into stack the same size of kernel buffer to copy)
    stackptr -= args_size;

    // Update the pointers in the kernel buffer with the starting stack position
    for(int i=0;i<argc;i++){
        kargs[i] = kargs[i] - kargs_ptr_start + (char *)stackptr + sizeof(char*)*(argc+1);
    }

    // Copy the whole kernel buffer into user stack
    copyout(kargs, (userptr_t)stackptr, args_size);

    enter_new_process(argc /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

    // enter_new_process does not return
	panic("enter_new_process in execv returned\n");

    err = EINVAL;

    return err;
}