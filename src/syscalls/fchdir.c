#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>

SYSCALL(29, fchdir,
int sys_fchdir(int fd) {
	if(unlikely(fd >= TASK_FD_COUNT)) {
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
