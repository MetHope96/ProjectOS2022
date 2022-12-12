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

void sys_exit(int exitcode){
    curproc->exit_code = _MKWAIT_EXIT(exitcode);
	thread_exit();
}