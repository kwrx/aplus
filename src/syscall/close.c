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


/**
 *	\brief This function shall deallocate the file descriptor indicated by fildes.\n
	To deallocate means to make the file descriptor available for return by subsequent calls
	to open() or other functions that allocate file descriptors.\n
	All outstanding record locks owned by the process on the file associated with the file
	descriptor shall be removed (that is, unlocked).
 *	\param fd File descriptor
 *	\return Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned 
			and errno set to indicate the error.
 */
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
