#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <libc.h>

SYSCALL(1, close,
int sys_close(int fd) {
    if(unlikely(fd < 0)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(fd >= TASK_FD_COUNT)) {
#if CONFIG_NETWORK
        return lwip_close(fd - TASK_FD_COUNT);
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

    current_task->fd[fd].inode = NULL;
    current_task->fd[fd].flags = 0;

    return vfs_close(inode);
});
