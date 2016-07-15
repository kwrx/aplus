#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>

SYSCALL(25, umount,
int sys_umount(const char* dev) {
	if((current_task->uid != TASK_ROOT_UID) && (current_task->gid != TASK_ROOT_GID)) {
		errno = EPERM;
		return -1;
	}

	inode_t* dev_ino = NULL;

	int devfd = sys_open(dev, O_RDONLY, 0);
	if(devfd >= 0) {
		dev_ino = current_task->fd[devfd].inode;
		sys_close(devfd);
	}

	return vfs_umount(dev_ino);
});

SYSCALL(26, umount2,
int sys_umount2(const char* dev, int flags) {
	(void) flags;

	return sys_umount(dev);
});
