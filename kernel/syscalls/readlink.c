#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(36, readlink,
ssize_t sys_readlink(const char* filename, char* buf, size_t bufsize) {
	if(unlikely(!filename || !buf || !bufsize)) {
		errno = EINVAL;
		return -1;
	}
	
	
	int fd = sys_open(filename, O_RDONLY | O_NOFOLLOW, 0);
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
			char b[BUFSIZ];
			char* p = b;
			int i;
			int j;
			
			
			inode_t* tmp;
			for(tmp = inode->link; tmp->parent; tmp = tmp->parent) {
				if(tmp == current_task->root)
					break;
					
				for(i = strlen(tmp->name) - 1; i >= 0; i--)
					*p++ = tmp->name[i];
					
				*p++ = '/';
			}
			
			if(p == b)
				*p++ = '/';
			*p++ = '\0';
			
			for(i = 0, j = strlen(b) - 1; i < bufsize && j >= 0; i++, j--)
				buf[i] = b[j];
				
			buf[i++] = '\0';
			return i;
		}
	}

	errno = EINVAL;
	return -1;		
});
