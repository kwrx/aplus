#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/syscall.h>
#include <aplus/task.h>


#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

extern task_t* current_task;


int sys_fchdir(int fd) {
	if(fd < 0 || fd > TASK_MAX_FD) {
		errno = EBADF;
		return -1;
	}


	if(current_task->fd[fd] == NULL) {
		errno = EBADF;
		return -1;
	}


	current_task->cwd = current_task->fd[fd];
	return 0;
}


SYSCALL(sys_fchdir, 29);
