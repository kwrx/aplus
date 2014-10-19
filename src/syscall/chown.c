#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>


extern task_t* current_task;

int sys_chown(char* filename, uid_t owner, gid_t group) {
	if(!im_superuser()) {
		kprintf("sys_chown: only superuser allow to use this function.");
		
		errno = EACCES;
		return -1;
	}
	
	
	int fd = sys_open(filename, O_RDONLY, 0644);
	if(fd < 0) {
		kprintf("sys_chown: %s.", strerror(errno));
		return -1;
	}
	
	inode_t* ino = current_task->fd[fd];
	return fs_chown(ino, owner, group);
}


SYSCALL(sys_chown, 21);
