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
