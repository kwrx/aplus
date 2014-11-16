#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

#include <stdio.h>

extern task_t* current_task;
extern inode_t* vfs_root;


int sys_read(int fd, void* ptr, size_t size) {
	if(!current_task)
		return -1;
		
	if(fd < 0 || fd > TASK_MAX_FD) {
		errno = EBADF;
		return -1;
	}
	
	inode_t* ino = current_task->fd[fd];
	if(!ino) {
		errno = EBADF;
		return -1;
	}


	return fs_read(ino, ptr, size);
}

SYSCALL(sys_read, 11);
