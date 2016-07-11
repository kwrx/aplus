#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/network.h>
#include <xdev/debug.h>
#include <libc.h>

SYSCALL(11, read,
int sys_read(int fd, void* buf, size_t size) {
	if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
#if CONFIG_NETWORK
		return lwip_read(fd - TASK_FD_COUNT, buf, size);
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

	if(unlikely(!(
		!(current_task->fd[fd].flags & O_WRONLY) ||
		(current_task->fd[fd].flags & O_RDWR)
	))) {
		errno = EPERM;
		return -1;
	}


	return vfs_read(inode, buf, size);
});
