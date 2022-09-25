#include <types.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <proc.h>
#include <current.h>
#include <kern/unistd.h>
#include <endian.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <file_syscall.h>
#include <synch.h>
#include <kern/errno.h>
#include <limits.h>
#include <stat.h>
#include <copyinout.h>

int sys_open(const char *filename, int flags){
  char file_name[__NAME_MAX];
  bool append = false; // This is 0 if is not open in append mode
  int err = 0;
  size_t len;
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

  int i;

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

    	return 0;
}

int sys_read(int fd, void *buff, size_t buff_len){
  int err;
  if (fd < 0 || fd >= OPEN_MAX || curproc->file_table[fd] == NULL){
    return EBADF; // Fd is not a valid file descriptor
  }

  if (buff == NULL){
    return EFAULT; // Part or all of the address space pointed to by buf is invalid.
  }

  char *buffer = (char *)kmalloc(sizeof(*buff)*buff_len);

  struct iovec iov;
  struct uio kuio;
  lock_acquire (curproc->file_table[fd]->lock);
  uio_kinit(&iov, &kuio, buffer, buff_len, curproc->file_table[fd]->offset, UIO_READ);

  err = VOP_READ(curproc->file_table[fd]->vnode, &kuio);
  if (err){
	 lock_release(curproc->file_table[fd]->lock);
	 kfree(buffer);
	 return err;
  }

  curproc->file_table[fd]->offset = kuio.uio_offset;
  lock_release(curproc->file_table[fd]->lock);
  kfree(buffer);
  return 0;
}

int sys_write(int fd, const void *buff, size_t buff_len){
  return 0;
}

int sys_lseek(int fd, off_t pos, int whence){
  return 0;
}

int sys_close(int fd){
  if (fd < 0 || fd >= OPEN_MAX || curproc->file_table[fd] == NULL){
    return EBADF; // Fd is not a valid file descriptor
  }

  lock_destroy(curproc->file_table[fd]->lock);
  vfs_close(curproc->file_table[fd]->vnode);
  kfree(curproc->file_table[fd]);
	curproc->file_table[fd] = NULL;
  return 0;
}

int sys_dup2(int oldfd, int newfd){
  return 0;
}

int sys_chdir(const char *pathname){
  return 0;
}

int sys_getcwd(char *buff, size_t buff_len){
  return 0;
}
