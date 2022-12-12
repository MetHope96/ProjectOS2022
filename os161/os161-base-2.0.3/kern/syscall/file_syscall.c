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

int sys_write(int fd, userptr_t buff, size_t buff_len, int *retval){
  int err;
  if (fd < 0 || fd >= OPEN_MAX){
    return EBADF; //Fd is not a valid file descriptor
  }

/*  
  char *buffer = (char *)kmalloc(sizeof(*buff)*buff_len);//buff user, buffer kernel
  err = copyin((const_userptr_t)buff, buffer, buff_len);
  if(err) {
    kfree(buffer);
    return err;
  }
*/

  struct iovec iov;
  struct uio kuio;

  lock_acquire(curproc->file_table[fd]->lock);

//  uio_kinit(&iov, &kuio, buffer, buff_len, curproc->file_table[fd]->offset, UIO_WRITE);
    uio_uinit(&iov, &kuio, buff, buff_len, curproc->file_table[fd]->offset, UIO_WRITE);
    u.uio_space = curproc->p_addrspace;
	u.uio_segflg = UIO_USERSPACE; // for user space address

  err = VOP_WRITE (curproc->file_table[fd]->vnode, &kuio);
  if (err){
  //  kfree(buffer);
    return err;
  }

  *retval = buff_len - kuio.uio_resid; //calculate the amount of bytes written. 0 in case of success.


  curproc->file_table[fd]->offset = kuio.uio_offset;

  lock_release(curproc->file_table[fd]->lock);
//  kfree(buffer);
  return 0;
}