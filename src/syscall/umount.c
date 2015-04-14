#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/list.h>
#include <aplus/attribute.h>
#include <aplus/syscall.h>


#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include <errno.h>


extern task_t* current_task;

int sys_umount2(const char* file, int flags) {
	if(!file) {
		errno = EINVAL;
		return -1;
	}

	int fd = sys_open(file, O_RDONLY, 0644);
	if(fd < 0) {
		errno = ENOENT;
		return -1;
	}

	inode_t* ino = current_task->fd[fd];
	sys_close(fd);

	ino->readdir = NULL;
	ino->finddir = NULL;
	ino->creat = NULL;
	ino->unlink = NULL;
	ino->flush = NULL;
	ino->chown = NULL;

	ino->dev = (dev_t) 0;
	//ino->mode &= ~S_IFMT;

	return 0;
}

int sys_umount(const char* file) {
	return sys_umount2(file, 0);
}


SYSCALL(sys_umount, 25);
SYSCALL(sys_umount2, 26);
