#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_

#include <types.h>
#include <proc.h>
#include <synch.h>
#include <limits.h>
#include <mips/trapframe.h>


/*Define the extern struct defined in an other file (proc.h) */

extern struct proc *proc_table[PID_MAX];
extern int proc_counter;

int sys_getpid(pid_t *curproc_pid);

#endif
