#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>


SYSCALL(40, getdents,
int sys_getdents(int fd, struct dirent* buf, size_t size) {
	if(unlikely(fd > TASK_FD_COUNT)) {
		errno = EBADF;
		return -1;
	}

	inode_t* inode = current_task->fd[fd].inode;
	
	if(unlikely(!inode)) {
		errno = EBADF;
		return -1;
	}

	if(unlikely(!inode->childs))
		return 0;


	uintptr_t dd_loc = 0;
	uintptr_t dd_buf = (uintptr_t) buf;

	struct inode_childs* tmp;
	for(tmp = inode->childs; tmp; tmp = tmp->next) {
		if(
			dd_loc 				+
			sizeof(struct dirent) 		+
			strlen(tmp->inode->name) > size
		) return dd_loc;

		buf = (struct dirent*) (dd_buf + dd_loc);
		buf->d_ino = (long) tmp->inode->ino;
		buf->d_off = (off_t) dd_loc + sizeof(struct dirent) + strlen(tmp->inode->name);
		buf->d_reclen = (uint16_t) sizeof(struct dirent) + strlen(tmp->inode->name);
		strcpy(buf->d_name, tmp->inode->name);

		dd_loc += buf->d_reclen;
	}

	return dd_loc;
});


