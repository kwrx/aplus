#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/network.h>
#include <xdev/debug.h>
#include <libc.h>

SYSCALL(36, readlink,
ssize_t sys_readlink(const char* filename, char* buf, size_t bufsize) {
	if(unlikely(!filename || !buf || !bufsize)) {
		errno = EINVAL;
		return -1;
	}
	
	
	int fd = sys_open(filename, O_RDONLY, 0);
	if(fd < 0)
		return -1;

	inode_t* inode = current_task->fd[fd].inode;
	
	sys_close(fd);
	
	
	if(unlikely(!inode)) {
		errno = EBADF;
		return -1;
	}

	if(likely(S_ISLNK(inode->mode))) {
		if(likely(inode->link)) {
			strncpy(buf, inode->link->name, bufsize);


			return (strlen(inode->link->name) > bufsize)
				? bufsize
				: strlen(inode->link->name);		
		}
	} 

	errno = EINVAL;
	return -1;		
});
