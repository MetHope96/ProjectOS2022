#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_

#include <types.h>
#include <proc.h>
#include <synch.h>
#include <limits.h>
#include <mips/trapframe.h>

void sys_exit(void);

#endif