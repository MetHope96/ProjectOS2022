#ifndef _FILE_SYSCALL_H_
#define _FILE_SYSCALL_H_

#include <types.h>

struct file_handle {
        char *name;
        int flags;
        off_t offset;        // create a offset type for manage append
        struct lock *lock;   // create a lock struct
        struct vnode *vnode; // create a struct vnode
};

int sys_write(int fd, userptr_t buff, size_t buff_len, int *retval);

#endif