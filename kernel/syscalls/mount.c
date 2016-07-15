#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>

SYSCALL(24, mount,
int sys_mount(const char* dev, const char* dir, const char* fstype, unsigned long int options, const void* data) {
	if((current_task->uid != TASK_ROOT_UID) && (current_task->gid != TASK_ROOT_GID)) {
		errno = EPERM;
		return -1;
	}

	inode_t* dev_ino = NULL;
	inode_t* dir_ino = NULL;


	int devfd = sys_open(dev, O_RDONLY, 0);
	if(devfd >= 0) {
		dev_ino = current_task->fd[devfd].inode;
		sys_close(devfd);
	}

	int dirfd = sys_open(dir, O_RDONLY | O_CREAT, S_IFDIR | 0666);
	if(dirfd >= 0) {
		dir_ino = current_task->fd[dirfd].inode;
		sys_close(dirfd);
	}


	if(dir_ino == NULL)
		return -1;
	
#ifdef ENOTBLK
	if(!(S_ISBLK(dev_ino->mode))) {
		errno = ENOTBLK;
		return -1;
	}
#endif

	return vfs_mount(dev_ino, dir_ino, fstype);
});
