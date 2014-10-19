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

int sys_fstat(int fd, struct stat* st) {
	if(!st) {
		errno = EINVAL;
		return -1;
	}

	inode_t* ino = current_task->fd[fd];
	if(!ino) {
		kprintf("sys_fstat: %s.", strerror(errno));
		errno = EBADF;
		return -1;
	}
	
	st->st_mode = ino->mode;
	st->st_ino = ino->ino;
	st->st_dev = ino->dev;
	st->st_uid = ino->uid;
	st->st_gid = ino->gid;
	st->st_atime = ino->atime;
	st->st_ctime = ino->ctime;
	st->st_mtime = ino->mtime;
	st->st_nlink = ino->nlink;
	st->st_size = ino->size;
	st->st_rdev = ino->rdev;
	st->st_blksize = 512;
	st->st_blocks = st->st_size / st->st_blksize;
	
	return 0;
}


SYSCALL(sys_fstat, 4);
