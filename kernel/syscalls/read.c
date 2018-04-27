#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(11, read,
int sys_read(int fd, void* buf, size_t size) {
    if(unlikely(fd < 0)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(fd >= TASK_FD_COUNT)) {
#if CONFIG_NETWORK
        return lwip_read(fd - TASK_FD_COUNT, buf, size);
#else
        errno = EBADF;
        return -1;
#endif
    }

    inode_t* inode = current_task->fd[fd].inode;
    
    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(!(
        !(current_task->fd[fd].flags & O_WRONLY) ||
        (current_task->fd[fd].flags & O_RDWR)
    ))) {
        errno = EPERM;
        return -1;
    }


    if(unlikely(current_task->fd[fd].flags & O_NONBLOCK)) {
        struct pollfd p;
        p.fd = fd;
        p.events = POLLIN;
        p.revents = 0;

        if(sys_poll(&p, 1, 0) < 0) {
            errno = EIO;
            return -1;
        }

        if(!(p.revents & POLLIN)) {
            errno = EAGAIN;
            return 0;
        }
    }


    current_task->iostat.rchar += (uint64_t) size;
    current_task->iostat.syscr += 1;


    register int e = vfs_read(inode, buf, current_task->fd[fd].position, size);
    if(unlikely(e <= 0))
        return 0;
    

    current_task->fd[fd].position += e;
    current_task->iostat.read_bytes += (uint64_t) e;
    return e;
});
