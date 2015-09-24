#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>

SYSCALL(21, chown,
int sys_chown(const char* pathname, uid_t owner, gid_t group) {
	int fd = sys_open(pathname, O_RDONLY, 0);
	if(fd < 0)
		return -1;

	inode_t* inode = current_task->fd[fd].inode;

	if((current_task->uid == inode->uid) || (current_task->uid == TASK_ROOT_UID))
		return vfs_chown(inode, owner, group);

	errno = EPERM;
	return -1;
});
