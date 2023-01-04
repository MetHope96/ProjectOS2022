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
#include <kern/seek.h>

int sys_open(const char *filename, int flags, int *retfd){
  bool append = false; // This is 0 if is not open in append mode
  int err = 0;
  size_t len = PATH_MAX;
  size_t actual;

  char *file_name;

  //Check if filename is invalid pointer
  if(filename == NULL){
    err = EFAULT;
    return err;
  }

  int copyinside = copyinstr((const_userptr_t)filename, file_name, len, &actual);
  if(copyinside){
    return copyinside;
  }

  switch(flags){
        case O_RDONLY: break;
        case O_WRONLY: break;
        case O_RDWR: break;
        // Create the file if it doesn't exist
        case O_CREAT|O_WRONLY: break;
        case O_CREAT|O_RDWR: break;
        // Create the file if it doesn't exist, fails if already exist
        case O_CREAT|O_EXCL|O_WRONLY: break;
        case O_CREAT|O_EXCL|O_RDWR: break;
        // Truncate the file to length 0 upon open
        case O_TRUNC|O_WRONLY: break;
        case O_CREAT|O_TRUNC|O_WRONLY: break;
        case O_TRUNC|O_RDWR: break;
        // Write at the end of the file
        case O_WRONLY|O_APPEND:
            append = true;
            break;
        case O_RDWR|O_APPEND:
            append = true;
            break;
        // EINVAL = flags contained invalid values
        default:
            err = EINVAL;
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
    err = vfs_open(file_name, flags, 0664, &curproc->file_table[i]->vnode);

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
    
    curproc->file_table[i]->flags = flags;
    curproc->file_table[i]->lock = lock_create("lock_fh"); //Create a lock for a file_handle
    if(curproc->file_table[i]->lock == NULL) {
      vfs_close(curproc->file_table[i]->vnode);
    	kfree(curproc->file_table[i]);
    	curproc->file_table[i] = NULL;
    }
      *retfd = i;
    	return 0;
}


int sys_write(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  if (fd < 0 || fd >= OPEN_MAX){
    return EBADF; //Fd is not a valid file descriptor
  }

  
  char *buffer = (char *)kmalloc(sizeof(*buff)*buff_len);//buff user, buffer kernel
  err = copyin((const_userptr_t)buff, buffer, buff_len);
  if(err) {
    kfree(buffer);
    return err;
  }
  

  struct iovec iov;
  struct uio kuio;

  lock_acquire(curproc->file_table[fd]->lock);
  uio_kinit(&iov, &kuio, buffer, buff_len, curproc->file_table[fd]->offset, UIO_WRITE);
  kuio.uio_space = curproc->p_addrspace;
  kuio.uio_segflg = UIO_USERSPACE; // for user space address 
  
  err = VOP_WRITE (curproc->file_table[fd]->vnode, &kuio);
  if (err){
    kfree(buffer);
    return err;
  }

  *retval = buff_len - kuio.uio_resid; //calculate the amount of bytes written. 0 in case of success.

  curproc->file_table[fd]->offset = kuio.uio_offset;
  lock_release(curproc->file_table[fd]->lock);
  kfree(buffer);
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
  kuio.uio_space = curproc->p_addrspace;
  kuio.uio_segflg = UIO_USERSPACE; // for user space address

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

    if (pos < 0) {
      curproc->file_table[fd]->offset = posPointer + pos; //Came back by EOF of abs(pos)
    } else {
      curproc->file_table[fd]->offset = posPointer; //Is equal to position of EOF
    }

    *retval = curproc->file_table[fd]->offset; //Return posPointer + pos or posPointer depend of the case
    break;

    default:
      return EINVAL; // A negative value of seek position
  }

  lock_release(curproc->file_table[fd]->lock);
  return 0;

}


int sys_dup2(int oldfd, int newfd) {
  //Check if the oldfd and newfd is a valid parameters
  if (oldfd >= OPEN_MAX || oldfd < 0 || newfd >= OPEN_MAX || newfd < 0){
    return EBADF; //oldfd is not a valid file handle or new fd is a value that can not be a valid file handle
  }
  struct file_handle *fpointer;
  fpointer = curproc->file_table[oldfd];
  //Check if oldfd is already opened
  if (curproc->file_table[oldfd] == NULL) {
    return EBADF; //oldfd is not a valid file handle or new fd is a value that can not be a valid file handle
  }

  //If the newfd is "free" copy the oldfd into newfd
  if (curproc->file_table[newfd] != NULL) {
    // if newfd is "open" need to close first
    sys_close(newfd);
  }
    curproc->file_table[newfd] = fpointer;
  return 0;
}


int sys_chdir(char *pathname){

  char newPathName[NAME_MAX]; // NAME_MAX = 255
  size_t actual;
  int err;

  if (pathname == NULL) {
    return EFAULT; // Part or all of the address space pointed to by buf is invalid.
  }
  //copyinstr
  if ((err =  copyinstr((const_userptr_t)pathname, newPathName, NAME_MAX, &actual) != 0)){
		return err;
	}

  err = vfs_chdir(pathname); // Set current directory, as a pathname.
  return err;

}


int sys_getcwd(char *buff, size_t buff_len){
  struct iovec iov;
  struct uio kuio;
  int err;

    if (buff == NULL) {
    return EFAULT; // Part or all of the address space pointed to by buf is invalid.
  }

  uio_kinit(&iov, &kuio, buff, buff_len-1, 0, UIO_READ); //offset = 0
  kuio.uio_segflg = UIO_USERSPACE; //Set what kind of pointer we have (userspace or kernelspace)
  kuio.uio_space = curproc->p_addrspace; //Address space for user pointer

  err = vfs_getcwd(&kuio); // Get current directory, as a pathname.

  if (err != 0){
    return err;
  }

  buff[sizeof(buff)-1-kuio.uio_resid] = 0;

  return 0;
}
