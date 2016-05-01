#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/network.h>
#include <xdev/debug.h>
#include <libc.h>

SYSCALL(17, write,
int sys_write(int fd, void* buf, size_t size) {
	kprintf(LOG, "sys_write(%d, %p, %d)\n", fd, buf, size);
	if(unlikely(fd >= TASK_FD_COUNT)) {
#if CONFIG_NETWORK
		return lwip_write(fd - TASK_FD_COUNT, buf, size);
#else
		errno = EBADF;
		return -1;
#endif
	}

	if(fd < 3)
		return kprintf(WARN, "%s", buf);

	inode_t* inode = current_task->fd[fd].inode;
	
	if(unlikely(!inode)) {
		errno = EBADF;
		return -1;
	}

	if(unlikely(!(
		(current_task->fd[fd].flags & O_WRONLY)	||
		(current_task->fd[fd].flags & O_RDWR)
	))) {
		errno = EPERM;
		return -1;
	}


	return vfs_write(inode, buf, size);
});
