#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>


extern task_t* current_task;


/**
 * 	\brief Changes root directory.
 *	\param filename
 *	\return On success, zero is returned. On error, −1 is returned, and errno is set appropriately.
 */
int sys_chroot(char* filename) {
	if(!im_superuser()) {
		kprintf("sys_chroot: only superuser allow to use this function.");
		
		errno = EACCES;
		return -1;
	}
	
	
	int fd = sys_open(filename, O_RDONLY, 0644);
	if(fd < 0)
		return -1;
	
	
	current_task->root = current_task->fd[fd];
	return 0;
}


SYSCALL(sys_chroot, 39);
