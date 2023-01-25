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

struct lock *arg_lock;

int sys_getpid(pid_t *curproc_pid) {
	*curproc_pid = curproc->proc_id;
	return 0;
}

static void op_efp(void *tfv, unsigned long data2){
  (void) data2;
  struct trapframe *tf = (struct trapframe *)tfv;
  enter_forked_process(tf);
}

int sys_fork(pid_t *child_pid, struct trapframe *tf){

  struct trapframe *tf_child; //Stack of the exception handled
  struct proc *child_proc;
  int err;
  char *name;

  if (proc_counter >= PID_MAX){
    return ENPROC; //There are too many process on the system.
  }
  name = curproc->p_name;
  child_proc = proc_create_runprogram(name);

  if(child_proc == NULL){
    return ENOMEM; //Sufficient virtual memory for the new process was not available.
  }



  /* Search the first free space in proc_table */

    if(proc_counter == MAX_PROC -1){
      return ENPROC; //There are already too many process on the system
    }

  proc_table[proc_counter] = child_proc;

  /*Copy the address space of the parent in a child process */
  err = as_copy(curproc->p_addrspace, &child_proc->p_addrspace);

  if(err){
    return err;
  }

  child_proc->parent_id = curproc->proc_id;

  /*Since the parent and child have the same file table: */

  for(int i = 0; i < OPEN_MAX; i++){
    if(curproc->file_table[i] != NULL){
      lock_acquire(curproc->file_table[i]->lock);//Lock the filetablei[i]
      child_proc->file_table[i] = curproc->file_table[i];
      lock_release(curproc->file_table[i]->lock);//Release the lock at filetable[i]
    }
  }

  spinlock_acquire(&curproc->p_lock);
      if (curproc->p_cwd != NULL) {
            VOP_INCREF(curproc->p_cwd);
            child_proc->p_cwd = curproc->p_cwd;
      }
  spinlock_release(&curproc->p_lock);

  /*Creation of memory space of trapframe */
  tf_child = (struct trapframe *)kmalloc(sizeof(struct trapframe));
  if(tf_child == NULL){
    return ENOMEM; //Sufficient virtual memory for the new process was not available.
  }

  /* Copy the trapframe into trapframe_child */
  //*tf_child = *tf;
  memcpy(tf_child, tf, sizeof(struct trapframe));

  err = thread_fork(curthread->t_name, child_proc,op_efp,(void *)tf_child, (unsigned long)0);
  /*If thread_fork fails: destroy the process and free the trapframe memory*/
  if(err){
    proc_destroy(child_proc);
    kfree(tf_child);
    return err;
  }

/*Set the return value as a child_PID */
  *child_pid = child_proc->proc_id;
  return 0;
}

void sys_exit(int exitcode){
	/*
	int i = 0;

	for(i = 0; i < MAX_PROC; i++){
		if(proc_table[i] != NULL){
			if(proc_table[i]->proc_id == curproc->proc_id)
			break;
		}
		if(i == MAX_PROC - 1)
		panic("Current process not found in process table");
	}

	lock_acquire(curproc->lock);
	curproc->exit_status = 1;
	curproc->exit_code = _MKWAIT_EXIT(exitcode);
	KASSERT(curproc->exit_status == proc_table[i]->exit_status);
	KASSERT(curproc->exit_code == proc_table[i]->exit_code);
	lock_release(curproc->lock);
	thread_exit();
	*/
    struct proc *p = curproc;
    pid_t pid = p->proc_id;

    // Store the passed exit code (readable by sys_waitpid)
    p->exit_code = exitcode;

    // Current process is ready to exit
    p->exit_status = true;

    // Process table is free
    proc_table[pid] = NULL;

    // Causes the current thread to exit by detaching it from process.
    // The process is made zombie (waiting for its removal but still in memory)
    thread_exit();
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t* retval) {

	int i = 0;
	int err = 0;
	if(options != 0){
		return EINVAL;
	}
	if(curproc->proc_id == pid){
		return ECHILD;
	}
	
	for(i = 0; i < OPEN_MAX; i++){
		if(proc_table[i] != NULL){
			if(proc_table[i]->proc_id == pid)
			break;
		}
		if(i == OPEN_MAX - 1)
		return ESRCH;
	}

	if(proc_table[i]->proc_id != 1){
	if(proc_table[i]->parent_id != curproc->proc_id){
		return ECHILD;
	}}
	
	KASSERT(proc_table[i] != NULL);
	lock_acquire(proc_table[i]->lock);
	if(proc_table[i]->exit_status){
		if(status != NULL) {
			err = copyout(&proc_table[i]->exit_code, (userptr_t)status, sizeof(proc_table[i]->exit_code));
			if(err){
				lock_release(proc_table[i]->lock);
				proc_destroy(proc_table[i]);
				proc_table[i] = NULL;
				return err;
			}
		}
		lock_release(proc_table[i]->lock);
		*retval = proc_table[i]->proc_id;
		proc_destroy(proc_table[i]);
		proc_table[i] = NULL;
		return 0;
	}

	cv_wait(proc_table[i]->cv, proc_table[i]->lock);
	if(status != NULL) {
		err = copyout(&proc_table[i]->exit_code, (userptr_t)status, sizeof(proc_table[i]->exit_code));
		if(err){
			lock_release(proc_table[i]->lock);
			proc_destroy(proc_table[i]);
			proc_table[i] = NULL;
			return err;
		}
	}
	lock_release(proc_table[i]->lock);
	*retval = proc_table[i]->proc_id;

	proc_destroy(proc_table[i]);
	proc_table[i] = NULL;

	return 0;
}

int sys_execv(const char *program, char **args1) {

	int err = 0;
	int i = 0;
	size_t got = 0;
	int extra_vals = 0;
	int padding = 0;
	int arg_counter = 0;
	struct addrspace *as;
	struct addrspace *old_as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	int current_position = 0;
	int bytes_remaining = ARG_MAX;
	userptr_t argv_ex = NULL;
	char prog_name[PATH_MAX];
	char *args;
		
	err = copyin((const_userptr_t)args1, &args, sizeof(args));
	if(err){
		return err;
	}
	lock_acquire(arg_lock);	
	err = copyinstr((const_userptr_t)program, prog_name, PATH_MAX, &got);
	if (err) {
		lock_release(arg_lock);
		return err;
	}
	while(args1[arg_counter] != NULL){
		err = copyinstr((const_userptr_t)args1[arg_counter], &arguments[current_position], bytes_remaining, &got);
		if(err) {
			lock_release(arg_lock);
			return err;
		}
		arg_pointers[arg_counter] = current_position; 
		current_position += got;
		extra_vals = got%4;
		if(extra_vals != 0) {
			padding = 4 - extra_vals;
			for(i = 0; i < padding; i++) {
				arguments[current_position + i] = '\0';
			}
			current_position += padding;
			bytes_remaining -= (got + padding);
		}else {
			bytes_remaining -= got;
		}
		arg_counter++;
		
	}
			
	result = vfs_open(prog_name, O_RDONLY, 0, &v);
	if (result) {
		lock_release(arg_lock);
		return result;
	}
	
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	old_as = proc_setas(as);
	as_destroy(old_as);
	as_activate();
	
	result = load_elf(v, &entrypoint);
	if (result) {
		lock_release(arg_lock);
		as_destroy(as);
		as_activate();
		vfs_close(v);
		return result;
	}
	
	vfs_close(v);

	result = as_define_stack(as, &stackptr);
	if(result) {
		lock_release(arg_lock);
		return result;
	}
	
	KASSERT(current_position % 4 == 0);
	stackptr -= current_position;
	copyout(arguments, (userptr_t)stackptr, current_position);

	stackptr -= 4;
	char *temp = NULL;
	copyout(&temp, (userptr_t)stackptr, sizeof(temp));
	
	for(i = arg_counter-1 ; i >= 0; i--) {
		stackptr -= 4;
		int val = USERSTACK - (current_position - arg_pointers[i]);
		temp = (char *)val;
		copyout(&temp, (userptr_t)stackptr, sizeof(temp));
	}

	argv_ex = (userptr_t)stackptr;
	stackptr = stackptr - 4;
	lock_release(arg_lock);
	enter_new_process(arg_counter, argv_ex, NULL, stackptr, entrypoint);
	return EINVAL;
}