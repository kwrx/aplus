#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>

SYSCALL(29, fchdir,
int sys_fchdir(int fd) {
    if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
        errno = EBADF;
        return -1;
    }

    inode_t* inode = current_task->fd[fd].inode;
    
    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(!(S_ISDIR(inode->mode)))) {
        errno = ENOTDIR;
        return -1;
    }

    current_task->cwd = inode;
    return 0;
});
