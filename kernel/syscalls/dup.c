#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <libc.h>

SYSCALL(59, dup,
int sys_dup(int oldfd) {
    if(oldfd < 0 || oldfd >= TASK_FD_COUNT) {
        errno = EBADF;
        return -1;
    }

    if(!current_task->fd[oldfd].inode) {
        errno = EBADF;
        return -1;
    }

    int fd = 0;
    while((fd < TASK_FD_COUNT) && (current_task->fd[fd].inode))
        fd++;

    if(fd >= TASK_FD_COUNT) {
        errno = EMFILE;
        return -1;
    }

    memcpy((void*) &current_task->fd[fd], (void*) &current_task->fd[oldfd], sizeof(fd_t));
    return fd;
});