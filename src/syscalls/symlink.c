#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>


SYSCALL(28, symlink,
int sys_symlink(const char* oldname, const char* newname) {
	if(unlikely(!oldname || !newname)) {
		errno = EINVAL;
		return -1;
	}

	int sfd = sys_open(oldname, O_RDONLY, 0);
	if(sfd < 0) {
		errno = ENOENT;
		return -1;
	}


	int dfd = sys_open(newname, O_EXCL | O_CREAT | O_RDONLY, S_IFLNK);
	if(dfd < 0)
		return -1;
	
	inode_t* sino = current_task->fd[sfd].inode;
	inode_t* dino = current_task->fd[dfd].inode;


	sino->nlink += 1;
	dino->link = sino;

	sys_close(dfd);
	sys_close(sfd);

	return 0;
});
