#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_

#include <types.h>
#include <proc.h>
#include <synch.h>
#include <limits.h>
#include <mips/trapframe.h>


/*Define the extern struct defined in an other file (proc.h) */
extern struct proc *proc_table[MAX_PROC];
extern int proc_counter;

int sys_getpid(pid_t *curproc_pid);
int sys_fork(pid_t *child_pid, struct trapframe *tf);
int sys_waitpid(pid_t pid, int *status, int options, pid_t* retval);
void sys_exit(int exitcode);
int sys_execv(char *program, char **args);

#endif