#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>


static int __check_perm(int uid, int gid, int other, mode_t mode, int flags) {
	if(uid)
		return ((flags & O_RDONLY) ? (mode & S_IRUSR) : 1) &&
			((flags & O_WRONLY) ? (mode & S_IWUSR) : 1) &&
			((flags & O_RDWR) ? ((mode & S_IRUSR) && (mode & S_IWUSR)) : 1);

	if(gid)
		return ((flags & O_RDONLY) ? (mode & S_IRGRP) : 1) &&
			((flags & O_WRONLY) ? (mode & S_IWGRP) : 1) &&
			((flags & O_RDWR) ? ((mode & S_IRGRP) && (mode & S_IWGRP)) : 1);


	return ((flags & O_RDONLY) ? (mode & S_IROTH) : 1) &&
		((flags & O_WRONLY) ? (mode & S_IWOTH) : 1) &&
		((flags & O_RDWR) ? ((mode & S_IROTH) && (mode & S_IWOTH)) : 1);
}




SYSCALL(10, open,
int sys_open(const char* name, int flags, mode_t mode) {
	char namebuf[strlen(name) + 1];
	strcpy(namebuf, name);

	char* s = namebuf;
	char* p = NULL;

	inode_t* cino = NULL;

	
	mode |= ~current_task->umask;

	if(s[0] == '/') {
		cino = current_task->root;
		s++;
	} else
		cino = current_task->cwd;

	KASSERT(cino);


	
	do {
		if((p = strchr(s, '/')))
			*p++ = '\0';
		else
			break;

		cino = vfs_finddir(cino, s);
		if(unlikely(!cino)) {
			errno = ENOENT;
			return -1;
		}

		if(S_ISLNK(cino->mode)) {
			if(cino->link) {
				if(cino->link == cino) {
					errno = ELOOP;
					return -1;
				}

				cino = cino->link;
			}
		}

		s = p;
	} while(s);


	inode_t* cp = cino;

	if(*s)
		cino = vfs_finddir(cp, s);

	if(!cino) {
		if(flags & O_CREAT) {
			cino = vfs_mknod(cp, s, mode);

			if(!cino) {
				errno = EROFS;
				return -1;
			}
		} else {
			errno = ENOENT;
			return -1;
		}
	} else {
		if((flags & O_EXCL) && (flags && O_CREAT)) {
			errno = EEXIST;
			return -1;
		}
	}


	if(
#ifdef O_NOFOLLOW
		!(flags & O_NOFOLLOW) && 
#endif
		S_ISLNK(cino->mode)
	) {
		if(cino->link) {
			if(cino->link == cino) {
				errno = ELOOP;
				return -1;
			}

			cino = cino->link;
		}
	}

#ifdef O_DIRECTORY
	if(flags & O_DIRECTORY) {
		if(!(S_ISDIR(cino->mode))) {
			errno = ENOTDIR;
			return -1;
		}
	}
#endif

	
	int r;
	if(cino->uid == current_task->uid)
		r = __check_perm(1, 0, 0, cino->mode, flags);
	else if(cino->gid == current_task->gid)
		r = __check_perm(0, 1, 0, cino->mode, flags);
	else
		r = __check_perm(0, 0, 1, cino->mode, flags);



	if(unlikely(!r)) {
		errno = EACCES;
		return -1;
	}



	if(vfs_open(cino) == E_ERR)
		return -1;
	


	int fd = 0;
	while((current_task->fd[fd].inode) && (fd < TASK_FD_COUNT))
		fd++;

	if(fd >= TASK_FD_COUNT) {
		errno = EMFILE;
		return -1;
	}

	current_task->fd[fd].inode = cino;
	current_task->fd[fd].flags = flags;

	return fd;
});
