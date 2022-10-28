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

int sys_open(char *filename, int flags, int *retfd){
  bool append = false; // This is 0 if is not open in append mode
  int err = 0;

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

    curproc->file_table[i] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
    err = vfs_open(filename, flags, 0, &curproc->file_table[i]->vnode);

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
      *retfd = i;
    	return 0;
}

int sys_read(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  if (fd < 0 || fd >= OPEN_MAX || curproc->file_table[fd] == NULL){
    return EBADF; // Fd is not a valid file descriptor
  }

  if (buff == NULL){
    return EFAULT; // Part or all of the address space pointed to by buf is invalid.
  }

  struct iovec iov;
  struct uio kuio;
  lock_acquire (curproc->file_table[fd]->lock);

  uio_kinit(&iov, &kuio, buff, buff_len, curproc->file_table[fd]->offset, UIO_READ);

  err = VOP_READ(curproc->file_table[fd]->vnode, &kuio);
  if (err){
	 lock_release(curproc->file_table[fd]->lock);
	 return err;
  }

  *retval = buff_len - kuio.uio_resid; //calculate the amount of bytes written. 0 in case of success.

  curproc->file_table[fd]->offset = kuio.uio_offset;
  lock_release(curproc->file_table[fd]->lock);

  return 0;
}

int sys_write(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  if (fd < 0 || fd >= OPEN_MAX || curproc->file_table[fd] == NULL){
    return EBADF; //Fd is not a valid file descriptor
  }

  /*
  char *buffer = (char *)kmalloc(sizeof(*buff)*buff_len);
  err = copyin((const_userptr_t)buff, buffer, buff_len);
  if(err) {
    kfree(buffer);
    return err;
  }
  */

  struct iovec iov;
  struct uio kuio;

  //void *buffer = (void *)buff;
  lock_acquire(curproc->file_table[fd]->lock);
  //uio_kinit(&iov, &kuio, buffer, buff_len, curproc->file_table[fd]->offset, UIO_WRITE);
  uio_kinit(&iov, &kuio, buff, buff_len, curproc->file_table[fd]->offset, UIO_WRITE);

  err = VOP_WRITE (curproc->file_table[fd]->vnode, &kuio);
  if (err){
    //kfree(buffer);
    return err;
  }

  *retval = buff_len - kuio.uio_resid; //calculate the amount of bytes written. 0 in case of success.

  curproc->file_table[fd]->offset = kuio.uio_offset;
  lock_release(curproc->file_table[fd]->lock);
  //kfree(buffer);
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


int sys_lseek(int fd, off_t pos, int whence, off_t *retval){

  int err;

  struct stat buffer;

  off_t currentPointer;
  off_t endPointer;
  off_t posPointer;

  if (fd < 0 || fd >= OPEN_MAX || curproc->file_table[fd] == NULL){
    return EBADF; //Fd is not a valid file descriptor
  }

  lock_acquire(curproc->file_table[fd]->lock); //lock the file

  switch(whence) {
    case SEEK_SET:
      if (pos < 0){
        lock_release(curproc->file_table[fd]->lock);
        return EINVAL; // A negative value of seek position
      }

      posPointer = pos;

      err = VOP_TRYSEEK(curproc->file_table[fd]->vnode, posPointer);
      if (err != 0){
        lock_release(curproc->file_table[fd]->lock);
        return ESPIPE; // Fd refers to an object which does not support seeking.
      }

      curproc->file_table[fd]->offset = posPointer;
      *retval = posPointer;
      break;

    case SEEK_CUR:
      currentPointer = curproc->file_table[fd]->offset;
      posPointer = currentPointer + pos;

      if (posPointer < 0){
        lock_release(curproc->file_table[fd]->lock);
        return EINVAL; // A negative value of seek position
      }

      err = VOP_TRYSEEK(curproc->file_table[fd]->vnode, posPointer);
      if (err != 0){
        lock_release(curproc->file_table[fd]->lock);
        return ESPIPE; // Fd refers to an object which does not support seeking.
      }

      curproc->file_table[fd]->offset = posPointer;
      *retval = posPointer;
      break;

    case SEEK_END:
    VOP_STAT(curproc->file_table[fd]->vnode, &buffer);
    endPointer = buffer.st_size;
    posPointer = endPointer + pos;

    if (posPointer < 0){
      lock_release(curproc->file_table[fd]->lock);
      return EINVAL; // A negative value of seek position
    }

    err = VOP_TRYSEEK(curproc->file_table[fd]->vnode, posPointer);
    if (err != 0){
      lock_release(curproc->file_table[fd]->lock);
      return ESPIPE; // Fd refers to an object which does not support seeking.
    }
    if (pos < 0) {
      curproc->file_table[fd]->offset = posPointer + pos; //Came back by EOF of abs(pos)
    } else {
      curproc->file_table[fd]->offset = posPointer; //Is equal to position of EOF
    }

    *retval = curproc->file_table[fd]->offset; //Return posPointer + pos or posPointer depend of the case
    break;

    default:
      return EINVAL;
  }

  lock_release(curproc->file_table[fd]->lock);
  return 0;

}

/*
int sys_dup2(int oldfd, int newfd){
  return 0;
}

int sys_chdir(const char *pathname){
  return 0;
}

int sys_getcwd(char *buff, size_t buff_len){
  return 0;
}
*/
