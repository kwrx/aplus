#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(17, write,
int sys_write(int fd, void* buf, size_t size) {
	current_task->iostat.wchar += (uint64_t) size;
	current_task->iostat.syscw += 1;

	if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
#if CONFIG_NETWORK
		return lwip_write(fd - TASK_FD_COUNT, buf, size);
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
		(current_task->fd[fd].flags & O_WRONLY)	||
		(current_task->fd[fd].flags & O_RDWR)
	))) {
		errno = EPERM;
		return -1;
	}


	register int e = vfs_write(inode, buf, size);
	if(unlikely(e <= 0))
		return 0;
	
	current_task->iostat.write_bytes += (uint64_t) e;
	return e;
});
