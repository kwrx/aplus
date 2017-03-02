#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>

SYSCALL(4, fstat,
int sys_fstat(int fd, struct stat* st) {
	if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
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
