#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>

SYSCALL(42, chmod,
int sys_chmod(const char* pathname, mode_t mode) {
    int fd = sys_open(pathname, O_RDONLY, 0);
    if(fd < 0)
        return -1;

    inode_t* inode = current_task->fd[fd].inode;
    sys_close(fd);

    if(!((current_task->uid == inode->uid) || (current_task->uid == TASK_ROOT_UID))) {
        errno = EPERM;
        return -1;
    }
    
    return vfs_chmod(inode, mode);
});
