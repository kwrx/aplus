#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>


SYSCALL(55, fsync,
int sys_fsync(int fd) {
    if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
        errno = EBADF;
        return -1;
    }

    inode_t* inode = current_task->fd[fd].inode;
    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }

    return vfs_fsync(inode);
});

SYSCALL(54, sync,
void sys_sync(void) {
    inline void flush_inode(inode_t* inode) {
        struct inode_childs* cx;
        for(cx = inode->childs; cx; cx = cx->next)
            flush_inode(cx->inode);

        if(inode->dirty)
            vfs_fsync(inode);
    }


    flush_inode(vfs_root);
});
