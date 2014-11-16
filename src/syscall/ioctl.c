#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

extern task_t* current_task;

int sys_ioctl(int fd, int req, void* buf) {

	if(fd < 0 || fd > TASK_MAX_FD) {
		errno = EBADF;
		return -1;
	}
	

	inode_t* ino = current_task->fd[fd];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	

	return fs_ioctl(ino, req, buf);
}


SYSCALL(sys_ioctl, 20);
