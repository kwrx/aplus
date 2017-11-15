#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(9, lseek,
off_t sys_lseek(int fd, off_t off, int dir) {
    if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
        errno = EBADF;
        return -1;
    }

    inode_t* inode = current_task->fd[fd].inode;

    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }


    if(S_ISFIFO(inode->mode)) {
        errno = ESPIPE;
        return -1;
    }

    switch(dir) {
        case SEEK_SET:
            current_task->fd[fd].position = off;
            break;
        case SEEK_CUR:
            current_task->fd[fd].position += off;
            break;
        case SEEK_END:
            current_task->fd[fd].position = off + inode->size;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return current_task->fd[fd].position;
});
