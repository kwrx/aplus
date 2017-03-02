#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <libc.h>

SYSCALL(19, fcntl,
int sys_fcntl(int fd, int cmd, long arg) {
	if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
#if CONFIG_NETWORK
		return lwip_fcntl(fd - TASK_FD_COUNT, cmd, arg);
#else
		errno = EBADF;
		return -1;
#endif
	}

	switch(cmd) {
		case F_DUPFD:
			if(fd == (int) arg)
				return (int) arg;
				
			sys_close(arg);
			memcpy((void*) &current_task->fd[arg], (void*) &current_task->fd[fd], sizeof(fd_t));
	
			return (int) arg;
		case F_GETFD:
			return 0;
		case F_SETFD:
			return 0;
		case F_GETFL:
			return current_task->fd[fd].flags;
		case F_SETFL:
			current_task->fd[fd].flags |= arg;
			return 0;
		default:
			errno = ENOSYS;
			return -1;
	}

	return 0;
});
