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

int sys_getpid(pid_t *curproc_pid) {
	*curproc_pid = curproc->proc_id;
	return 0;
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

  int j=0;

  /* Search the first free space in proc_table */
  while(proc_table[j] != NULL){
    if(j == MAX_PROC -1){
      return ENPROC; //There are already too many process on the system
    }
    j++;
  }
  proc_table[j] = child_proc;

  /*Copy the address space of the parent in a child process */
  err = as_copy(curproc->p_addrspace, &childproc->p_addrspace);

  if(err){
    return err;
  }

  proc_counter=proc_counter+1;
  child_proc->proc_id = proc_counter;
  child_proc->parent_id = curproc->proc_id;

  /*Since the parent and child have the same file table: */

  for(int i = 0; i < OPEN_MAX, i++){
    if(curproc->file_table[i] != NULL){
      lock_acquire(curproc->file_table[i]->lock);//Lock the filetablei[i]
      child_proc->file_table[i] = curproc->file_table[i];
      lock_release(curproc->file_table[i]->lock);//Release the lock at filetable[i]
    }
  }

  /*Creation of memory space of trapframe */
  tf_child = (struct trapframe *)kmalloc(sizeof(struct trapframe));
  if(tf_child == NULL){
    return ENOMEM; //Sufficient virtual memory for the new process was not available.
  }

  /* Copy the trapframe into trapframe_child */
  *tf_child = *tf;

  err = thread_fork(curthread->t_name, child_proc,enter_forked_process,(struct trapframe*)tf_child, (unsigned long)0);
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
