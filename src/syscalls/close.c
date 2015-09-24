#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/network.h>
#include <libc.h>

SYSCALL(1, close,
int sys_close(int fd) {
	if(unlikely(fd > TASK_FD_COUNT)) {
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
