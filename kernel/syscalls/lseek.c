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


    switch(dir) {
        case SEEK_SET:
            inode->position = (off64_t) off;
            break;
        case SEEK_CUR:
            inode->position += (off64_t) off;
            break;
        case SEEK_END:
            inode->position = (off64_t) off + (off64_t) inode->size;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return (off_t) inode->position;
});
