#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <errno.h>

extern task_t* current_task;

struct dirent* sys_readdir(int fd, int position) {
	if(!current_task)
		return NULL;


	if(fd < 0 || fd > TASK_MAX_FD) {
		errno = EBADF;
		return NULL;
	}
	
		
	
	inode_t* ino = current_task->fd[fd];
	if(!ino) {
		errno = EBADF;
		return NULL;
	}


	return (struct dirent*) fs_readdir(ino, position);
}

SYSCALL(sys_readdir, 80);
