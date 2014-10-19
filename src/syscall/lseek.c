#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

#include <stdio.h>

extern task_t* current_task;


int sys_lseek(int fd, int pos, int dir) {
	inode_t* ino = current_task->fd[fd];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	
	if(pos > ino->size)
		pos = ino->size;
	
	switch(dir) {
		case SEEK_SET:
			ino->position = pos;
			break;
		case SEEK_END:
			ino->position = ino->size - pos;
			break;
		case SEEK_CUR:
			ino->position = (ino->position + pos > ino->size ? ino->size : ino->position + pos);
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	
	return ino->position;
}

SYSCALL(sys_lseek, 9);
