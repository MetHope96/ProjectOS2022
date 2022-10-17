/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * fstest - filesystem test code
 *
 * Writes a file (in small chunks) and then reads it back again
 * (also in small chunks) and complains if what it reads back is
 * not the same.
 *
 * The length of SLOGAN is intentionally a prime number and
 * specifically *not* a power of two.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <synch.h>
#include <vfs.h>
#include <fs.h>
#include <vnode.h>
#include <test.h>
#include <file_syscall.h>

int test_open(int nargs, char **arg){
	(void)nargs;
	(void)args;
	char filename[50] = "prova.txt"
    int retvalue;
	int ind;
    int flag = O_WRONLY|O_CREAT;
	retvalue = sys_open(filename, flag, &ind);

	return retvalue;

	/*
  char file_name[__NAME_MAX];
  bool append = false; // This is 0 if is not open in append mode
  int err = 0;
  size_t len = __NAME_MAX;
  size_t actual;

  if (file_name == NULL){
    err = EFAULT; // File name was an invalid pointer
    return err;
  }

  switch(flags){ // Check if there are a valid flags
    case O_RDONLY:
      break;
    case O_WRONLY:
      break;
    case O_RDWR:
      break;
    case O_RDONLY|O_CREAT:
      break;
    case O_WRONLY|O_CREAT:
      break;
    case O_WRONLY|O_APPEND:
      append = true;
      break;
    case O_RDWR|O_CREAT:
      break;
    case O_RDWR|O_APPEND:
      append = true;
      break;
    default:
      err = EINVAL; // Flags contain invalid values
        return err;
  }

  int i=3;

    while (curproc->file_table[i] != NULL){
      if (i == OPEN_MAX-1){
        return EMFILE; //The process's file table was full, or a process-specific limit on open files was reached.
      }
      i++;
    }

    err = copyinstr((const_userptr_t)filename, file_name, len, &actual);
    if (err){
      return err;
    }

    curproc->file_table[i] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
    err = vfs_open(file_name, flags, 0, &curproc->file_table[i]->vnode);
    if (err){
      kfree(curproc->file_table[i]);
      curproc->file_table[i]=NULL;
      return err;
    }

    if(append){ // The file is open in append mode
      struct stat statbuf;
      err = VOP_STAT(curproc->file_table[i]->vnode, &statbuf);
      if (err){
        kfree (curproc->file_table[i]);
        curproc->file_table[i] = NULL;
        return err;
      }
      curproc->file_table[i]->offset = statbuf.st_size;

    } else { //The file isn't open in append mode
    curproc->file_table[i]->offset = 0;
    }

    curproc->file_table[i]->lock = lock_create("lock_fh"); //Create a lock for a file_handle
    if(curproc->file_table[i]->lock == NULL) {
      vfs_close(curproc->file_table[i]->vnode);
    	kfree(curproc->file_table[i]);
    	curproc->file_table[i] = NULL;
    }
      *retval = i;
    	return 0;
		*/
}
