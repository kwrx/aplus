#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

extern task_t* current_task;


int sys_close(int fd) {
	if(!current_task)
		return -1;
		
	if(current_task->fd[fd] == 0) {
		kprintf("sys_close: fd %d not exist.", fd);
		
		errno = EBADF;
		return -1;
	}
	
	current_task->fd[fd] = 0;
	return 0;
}

SYSCALL(sys_close, 1);
