#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(37, chdir,
int sys_chdir(const char* path) {
    int fd = sys_open(path, O_RDONLY, 0);
    if(fd < 0)
        return -1;

    inode_t* inode = current_task->fd[fd].inode;
    sys_close(fd);


    if(unlikely(!(S_ISDIR(inode->mode)))) {
        errno = ENOTDIR;
        return -1;
    }

    current_task->cwd = inode;
    return 0;
});
