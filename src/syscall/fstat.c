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
 * 	\brief This function shall obtain information about an open file associated with
		the file descriptor fildes, and shall write it to the area pointed to by buf.
 *	\param fd File descriptor
 *	\param st Stat structure
 *	\return Upon successful completion, 0 shall be returned.\n
			Otherwise, -1 shall be returned and errno set to indicate the error.
 */
int sys_fstat(int fd, struct stat* st) {
	if(unlikely(!st)) {
		errno = EINVAL;
		return -1;
	}

	if(unlikely(fd < 0 || fd > TASK_MAX_FD)) {
		errno = EBADF;
		return -1;
	}
	

	inode_t* ino = current_task->fd[fd];
	if(unlikely(!ino)) {
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
