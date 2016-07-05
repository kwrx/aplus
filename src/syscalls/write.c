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
	if(unlikely(fd >= TASK_FD_COUNT)) {
#if CONFIG_NETWORK
		return lwip_write(fd - TASK_FD_COUNT, buf, size);
#else
		errno = EBADF;
		return -1;
#endif
	}

	if(fd == 1 || fd == 2) {
		int i;
		for(i = 0; i < size; i++)
			kprintf(fd == 1 ? USER : ERROR, "%c", ((char*)buf)[i] & 0xFF);
			
		return size;
	}

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
