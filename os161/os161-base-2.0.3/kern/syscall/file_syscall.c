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
  struct proc *p = curproc;

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
    //Find the first empty slot of file table
    while (p->file_table[i] != NULL){
      if (i == OPEN_MAX-1){
        return EMFILE; //The process's file table was full, or a process-specific limit on open files was reached.
      }
      i++;
    }

    //Create the memory location for the element of file table
    p->file_table[i] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
    //Effective open of the file
    err = vfs_open(kfilename, flags, 0664, &p->file_table[i]->vnode);

    if (err){
      kfree(p->file_table[i]);
      p->file_table[i]=NULL;
      return err;
    } 

    lock_acquire(p->lock);

    if(append){ // The file is open in append mode
      struct stat statbuf;
      //get file information and offset
      err = VOP_STAT(p->file_table[i]->vnode, &statbuf);
      if (err){
        kfree (p->file_table[i]);
        p->file_table[i] = NULL;
        return err;
      }
      p->file_table[i]->offset = statbuf.st_size; //offset update

    } else { //The file isn't open in append mode
    p->file_table[i]->offset = 0;
    }
    //Initialization
    p->file_table[i]->ref_count = 1;
    p->file_table[i]->flags = flags;
    p->file_table[i]->lock = lock_create("lock_fh"); //Create a lock for a file_handle
    if(p->file_table[i]->lock == NULL) {
      vfs_close(p->file_table[i]->vnode);
    	kfree(p->file_table[i]);
    	p->file_table[i] = NULL;
    }

    lock_release(p->lock);

      *retfd = i;
    	return 0;
}


int sys_write(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  struct file_handle *fh;
  struct proc *p = curproc;

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

  fh = p->file_table[fd];

  lock_acquire(fh->lock);

  if (fd < 0 || fd >= OPEN_MAX){
    return EBADF; //Fd is not a valid file descriptor
  }
  
  // - Part or all of the address space pointed to by buf is invalid
  if(buff == NULL){
        err = EFAULT;
        return err;
  }
  
  struct iovec iov;//iovec structure, used in the readv/writev scatter/gather I/O calls, and within the kernel for keeping track of blocks of data for I/O.
  struct uio kuio;//Describe I/O operation

  uio_kinit(&iov, &kuio, buff, buff_len, fh->offset, UIO_WRITE); //Initialization uio struct
  kuio.uio_space = p->p_addrspace;
  kuio.uio_segflg = UIO_USERSPACE; // for user space address 

  err = VOP_WRITE (fh->vnode, &kuio);//Effective write
  if (err){
    return err;
  }

  *retval = buff_len - kuio.uio_resid; //calculate the amount of bytes written. buff_len in case of success.

  fh->offset = kuio.uio_offset;//Update offset
  lock_release(fh->lock);
  return 0;
}

int sys_read(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  struct file_handle *fh;
  struct proc *p = curproc;

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

  fh = p->file_table[fd];

  lock_acquire (fh->lock);

  if (fd < 0 || fd >= OPEN_MAX || fh == NULL){
    return EBADF; // Fd is not a valid file descriptor
  }

  if (buff == NULL){
    return EFAULT; // Part or all of the address space pointed to by buf is invalid.
  }

  struct iovec iov;//iovec structure, used in the readv/writev scatter/gather I/O calls, and within the kernel for keeping track of blocks of data for I/O.
  struct uio kuio;//Describe I/O operation

  uio_kinit(&iov, &kuio, buff, buff_len, fh->offset, UIO_READ);//Initialization uio struct
  kuio.uio_space = p->p_addrspace;
  kuio.uio_segflg = UIO_USERSPACE; // for user space address 

  err = VOP_READ(fh->vnode, &kuio);//Effective read
  if (err){
	 lock_release(fh->lock);
	 return err;
  }

  *retval = buff_len - kuio.uio_resid; //calculate the amount of bytes written. buff_len in case of success.

  fh->offset = kuio.uio_offset;//Update offset
  lock_release(fh->lock);

  return 0;
}

int sys_close(int fd){
  struct proc *p = curproc;
  lock_acquire(p->lock);

  if (fd < 0 || fd >= OPEN_MAX || p->file_table[fd] == NULL){
    return EBADF; // Fd is not a valid file descriptor
  }

  p->file_table[fd]->ref_count -- ;

//Destroy element of file table
  if(p->file_table[fd]->ref_count == 0){
  lock_destroy(p->file_table[fd]->lock);
  vfs_close(p->file_table[fd]->vnode);
  kfree(p->file_table[fd]);
	p->file_table[fd] = NULL;
  }

  lock_release(p->lock);

  return 0;
}


int sys_lseek(int fd, off_t pos, int whence, off_t *retval){
  struct proc *p = curproc;
  struct stat buffer;

  off_t currentPointer;
  off_t endPointer;
  off_t posPointer;

  if (fd < 0 || fd >= OPEN_MAX || p->file_table[fd] == NULL){
    return EBADF; //Fd is not a valid file descriptor
  }

  lock_acquire(p->file_table[fd]->lock); //lock the file

  switch(whence) {
    case SEEK_SET:
      if (pos < 0){
        lock_release(p->file_table[fd]->lock);
        return EINVAL; // A negative value of seek position
      }

      posPointer = pos;

      p->file_table[fd]->offset = posPointer;
      *retval = posPointer;
      break;

    case SEEK_CUR:
      currentPointer = p->file_table[fd]->offset;
      posPointer = currentPointer + pos;

      if (posPointer < 0){
        lock_release(p->file_table[fd]->lock);
        return EINVAL; // A negative value of seek position
      }

      p->file_table[fd]->offset = posPointer;
      *retval = posPointer;
      break;

    case SEEK_END:
    VOP_STAT(p->file_table[fd]->vnode, &buffer);//get file information and offset
    endPointer = buffer.st_size;
    posPointer = endPointer;

    if (posPointer + pos < 0){
      lock_release(p->file_table[fd]->lock);
      return EINVAL; // A negative value of seek position
    }

    if (pos < 0) {
      p->file_table[fd]->offset = posPointer + pos; //Came back by EOF of abs(pos)
    } else {
      p->file_table[fd]->offset = endPointer; //Is equal to position of EOF
    }

    *retval = p->file_table[fd]->offset; //Return posPointer + pos or posPointer depend of the case
    break;

    default:
      return EINVAL; // A negative value of seek position
  }

  lock_release(p->file_table[fd]->lock);
  return 0;

}

int 
sys_dup2(int oldfd, int newfd, int *retval){
  int err;
  struct proc *p = curproc;

  if(oldfd < 0 || newfd < 0 || oldfd >= OPEN_MAX || newfd >= OPEN_MAX){//Check valid fd
    err = EBADF;
    return err;
  }

  if (newfd == oldfd){
      *retval = newfd;
      return 0;
  }

  if(p->file_table[newfd] != NULL){
    err = sys_close(newfd);
    if(err)
      return err;
  }

  p->file_table[newfd] = p->file_table[oldfd];//new fd element points the same element of oldfd 
  p->file_table[oldfd]->ref_count++;//Update ref_count
  
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

    if(path != NULL && strlen((char*)path) > PATH_MAX){//Check valid path name
        err = ENAMETOOLONG;
        return err;
    }
    
    len = strlen((char*)path) + 1;//lenght of path name

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

    err = vfs_open( k_buf, O_RDONLY, 0644, &dir_vn ); //open directory vnode
	  if( err ){
        kfree(k_buf);
        return err;
    }else{
      *retval = 0;
    }

    err = vfs_setcurdir( dir_vn );//Set current directory as a vnode

	vfs_close( dir_vn );//close directory vnode

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
    struct proc *p = curproc;

    KASSERT(curthread != NULL);
    KASSERT(curproc != NULL );

    //Check buf points to a valid address
    if(buf == NULL){
        err = EFAULT;
        return err;
    }

    //Initialization uio struct
	uio_kinit(&iov, &kuio, buf, buf_len, 0, UIO_READ);
  kuio.uio_space = p->p_addrspace;
	kuio.uio_segflg = UIO_USERSPACE; // for user space address

    err = vfs_getcwd(&kuio);//Effective getcwd
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
  struct proc *p = curproc;

  char *kfilename;

  kfilename = kstrdup((char *)filename);
    if(kfilename==NULL){
        return ENOMEM;
    }

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

    p->file_table[fd] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
    err = vfs_open(kfilename, flags, 0, &p->file_table[fd]->vnode);

    if (err){
      kfree(p->file_table[fd]);
      p->file_table[fd]=NULL;
      return err;
    }

    lock_acquire(p->lock);
    p->file_table[fd]->ref_count = 1;
    p->file_table[fd]->offset = 0;
    p->file_table[fd]->flags = flags;
    p->file_table[fd]->lock = lock_create("lock_fh"); //Create a lock for a file_handle
    if(p->file_table[fd]->lock == NULL) {
      vfs_close(p->file_table[fd]->vnode);
    	kfree(p->file_table[fd]);
    	p->file_table[fd] = NULL;
    }
    lock_release(p->lock);

  return fd;

}