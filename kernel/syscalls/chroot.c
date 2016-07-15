#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>

SYSCALL(39, chroot,
int sys_chroot(const char* path) {
	int fd = sys_open(path, O_RDONLY, 0);
	if(fd < 0)
		return -1;

	inode_t* inode = current_task->fd[fd].inode;
	sys_close(fd);


	if(unlikely(!(S_ISDIR(inode->mode)))) {
		errno = ENOTDIR;
		return -1;
	}

	current_task->root = inode;
	return 0;
});
