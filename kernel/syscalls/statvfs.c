#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>

SYSCALL(62, fstatvfs,
int sys_fstatvfs(int fd, struct statvfs* st) {
    if(unlikely(!st)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(fd < 0 || fd > TASK_FD_COUNT)) {
        errno = EBADF;
        return -1;
    }

    inode_t* inode = current_task->fd[fd].inode;
    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(!inode->mtinfo)) {
        errno = ENOSYS;
        return -1;
    }

    memcpy(st, &inode->mtinfo->stat, sizeof(struct statvfs));
    return 0;
});


SYSCALL(61, statvfs,
int sys_statvfs(const char* path, struct statvfs* st) {
    if(unlikely(!path || !st)) {
        errno = EINVAL;
        return -1;
    }

    int fd = sys_open(path, O_RDONLY, 0);
    if(unlikely(fd < 0)) {
        errno = ENOENT;
        return -1;
    }

    int e = sys_fstatvfs(fd, st);

    sys_close(fd);
    return e;
});