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

int sys_open(userptr_t filename, int flags, int *retfd){
  bool append = false; // This is 0 if is not open in append mode
  int err = 0;

  char *kfilename;

  //Check if filename is invalid pointer
  if(filename == NULL){
    err = EFAULT;
    return err;
  }

    // Copy the filename string from user to kernel space to protect it
    kfilename = kstrdup((char *)filename);
    if(kfilename==NULL){
        return ENOMEM;
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
    err = vfs_open(kfilename, flags, 0664, &curproc->file_table[i]->vnode);

    if (err){
      kfree(curproc->file_table[i]);
      curproc->file_table[i]=NULL;
      return err;
    } 

    lock_acquire(curproc->lock);

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
    
    curproc->file_table[i]->ref_count = 1;
    curproc->file_table[i]->flags = flags;
    curproc->file_table[i]->lock = lock_create("lock_fh"); //Create a lock for a file_handle
    if(curproc->file_table[i]->lock == NULL) {
      vfs_close(curproc->file_table[i]->vnode);
    	kfree(curproc->file_table[i]);
    	curproc->file_table[i] = NULL;
    }

    lock_release(curproc->lock);

      *retfd = i;
    	return 0;
}


int sys_write(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  lock_acquire(curproc->lock);
  lock_acquire(curproc->file_table[fd]->lock);

  if (fd < 0 || fd >= OPEN_MAX){
    return EBADF; //Fd is not a valid file descriptor
  }
  
  // - Part or all of the address space pointed to by buf is invalid
  if(buff == NULL){
        err = EFAULT;
        return err;
  }
  
  struct iovec iov;
  struct uio kuio;

  uio_kinit(&iov, &kuio, buff, buff_len, curproc->file_table[fd]->offset, UIO_WRITE);
  kuio.uio_space = curproc->p_addrspace;
  kuio.uio_segflg = UIO_USERSPACE; // for user space address 

  err = VOP_WRITE (curproc->file_table[fd]->vnode, &kuio);
  if (err){
    return err;
  }

  *retval = buff_len - kuio.uio_resid; //calculate the amount of bytes written. 0 in case of success.

  curproc->file_table[fd]->offset = kuio.uio_offset;
  lock_release(curproc->file_table[fd]->lock);
  lock_release(curproc->lock);
  return 0;
}

int sys_read(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  lock_acquire (curproc->lock);
  lock_acquire (curproc->file_table[fd]->lock);

  if (fd < 0 || fd >= OPEN_MAX || curproc->file_table[fd] == NULL){
    return EBADF; // Fd is not a valid file descriptor
  }

  if (buff == NULL){
    return EFAULT; // Part or all of the address space pointed to by buf is invalid.
  }

  struct iovec iov;
  struct uio kuio;

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
  lock_release(curproc->lock);

  return 0;
}

int sys_close(int fd){

  lock_acquire(curproc->lock);

  if (fd < 0 || fd >= OPEN_MAX || curproc->file_table[fd] == NULL){
    return EBADF; // Fd is not a valid file descriptor
  }

  curproc->file_table[fd]->ref_count -- ;

  if(curproc->file_table[fd]->ref_count == 0){
  lock_destroy(curproc->file_table[fd]->lock);
  vfs_close(curproc->file_table[fd]->vnode);
  kfree(curproc->file_table[fd]);
	curproc->file_table[fd] = NULL;
  }

  lock_release(curproc->lock);

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

int 
sys_dup2(int oldfd, int newfd, int *retval){
  int err;

  if(oldfd < 0 || newfd < 0 || oldfd >= OPEN_MAX || newfd >= OPEN_MAX){
    err = EBADF;
    return err;
  }

  if (newfd == oldfd){
      *retval = newfd;
      return 0;
  }

  if(curproc->file_table[newfd] != NULL){
    err = sys_close(newfd);
    if(err)
      return err;
  }

  curproc->file_table[newfd] = curproc->file_table[oldfd];
  curproc->file_table[oldfd]->ref_count++;
  
  *retval = newfd;
  return 0;

}


int 
sys_chdir(userptr_t path, int *retval){
    int err, len;
    char* k_buf;
    struct vnode *dir_vn;
    
    if(path == NULL){
        err = EFAULT;
        return err;
    }

    if(path != NULL && strlen((char*)path) > PATH_MAX){
        err = ENAMETOOLONG;
        return err;
    }
    
    len = strlen((char*)path) + 1;

    k_buf = kmalloc(len * sizeof(char));
    if(k_buf == NULL){
        err = ENOMEM;
        return err;
    }

    err = copyinstr(path, k_buf, len, NULL);
    if (err){
        kfree(k_buf);
        return err;
    }

    err = vfs_open( k_buf, O_RDONLY, 0644, &dir_vn );
	if( err ){
        kfree(k_buf);
        return err;
    }else{
      *retval = 0;
    }

    err = vfs_setcurdir( dir_vn );

	vfs_close( dir_vn );

	if( err ){
        kfree(k_buf);
        return err;
    }
    kfree(k_buf);
    return 0;
}

int sys___getcwd(userptr_t buf, size_t buf_len, int *retval){

    int err;
    struct uio kuio;
    struct iovec iov;
    

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

    // - buf points to an invalid address.
    if(buf == NULL){
        err = EFAULT;
        return err;
    }

    // Setup the uio record (use a proper function to init all fields)
	uio_kinit(&iov, &kuio, buf, buf_len, 0, UIO_READ);
  kuio.uio_space = curproc->p_addrspace;
	kuio.uio_segflg = UIO_USERSPACE; // for user space address

    // Retrieve the uio struct associated with the current directory
    // (containing vnode and string with pathname)
    err = vfs_getcwd(&kuio);
    if(err)
        return err;

    // Actual lenght of the current pathname directory is returned
    *retval = buf_len - kuio.uio_resid;

    if(*retval < 0){
        err = EFAULT;
    }

    return 0;
}

int
std_open(int fileno){
  int fd;
  int err, flags;
  const char* filename = "con:";

  char file_name[5];

  strcpy(file_name, filename);

  switch(fileno){
    case STDIN_FILENO:
      flags = O_RDONLY;
      fd = STDIN_FILENO;
      break;
    case STDOUT_FILENO:
      flags = O_WRONLY;
      fd = STDOUT_FILENO;
      break;
    case STDERR_FILENO:
      flags = O_WRONLY;
      fd = STDERR_FILENO;
      break;
    default:
      return -1;
      break;
  }

    curproc->file_table[fd] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
    err = vfs_open(file_name, flags, 0, &curproc->file_table[fd]->vnode);

    if (err){
      kfree(curproc->file_table[fd]);
      curproc->file_table[fd]=NULL;
      return err;
    }

    lock_acquire(curproc->lock);
    curproc->file_table[fd]->ref_count = 1;
    curproc->file_table[fd]->offset = 0;
    curproc->file_table[fd]->flags = flags;
    curproc->file_table[fd]->lock = lock_create("lock_fh"); //Create a lock for a file_handle
    if(curproc->file_table[fd]->lock == NULL) {
      vfs_close(curproc->file_table[fd]->vnode);
    	kfree(curproc->file_table[fd]);
    	curproc->file_table[fd] = NULL;
    }
    lock_release(curproc->lock);

  return fd;

}