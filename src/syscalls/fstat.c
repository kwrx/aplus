#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>

SYSCALL(4, fstat,
int sys_fstat(int fd, struct stat* st) {
	if(unlikely(fd > TASK_FD_COUNT)) {
		errno = EBADF;
		return -1;
	}

	inode_t* inode = current_task->fd[fd].inode;
	
	if(unlikely(!inode)) {
		errno = EBADF;
		return -1;
	}

	if(unlikely(!st)) {
		errno = EINVAL;
		return -1;
	}

	st->st_dev = inode->dev;
	st->st_ino = inode->ino;
	st->st_mode = inode->mode;
	st->st_nlink = inode->nlink;
	st->st_uid = inode->uid;
	st->st_gid = inode->gid;
	st->st_rdev = inode->rdev;
	st->st_size = (off_t) inode->size;
	
	return 0;
});
