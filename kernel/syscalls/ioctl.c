#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/network.h>
#include <libc.h>

SYSCALL(20, ioctl,
int sys_ioctl(int fd, int req, void* arg) {
	if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
#if CONFIG_NETWORK
		return lwip_ioctl(fd - TASK_FD_COUNT, req, arg);
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

	return vfs_ioctl(inode, req, arg);
});
