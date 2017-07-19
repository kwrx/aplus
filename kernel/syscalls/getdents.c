#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
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

	memset(buf, 0, size);

	uintptr_t dd_loc = 0;
	uintptr_t dd_buf = (uintptr_t) buf;
	int p = 0;

	struct inode_childs* tmp;
	for(tmp = inode->childs; tmp; tmp = tmp->next) {
		if(
			dd_loc 						+
			sizeof(struct dirent)		+
			strlen(tmp->inode->name) > size
		) return dd_loc;
		
		if(p++ < inode->position)
			continue;

		buf = (struct dirent*) (dd_buf + dd_loc);
		buf->d_ino = (long) tmp->inode->ino;
		buf->d_type = (tmp->inode->mode & S_IFMT) >> 12;
		buf->d_off = (off_t) dd_loc + sizeof(struct dirent) + strlen(tmp->inode->name);
		buf->d_reclen = (uint16_t) sizeof(struct dirent) + strlen(tmp->inode->name);
		strcpy(buf->d_name, tmp->inode->name);
		

		dd_loc += buf->d_reclen;
		inode->position += 1;
	}

	return dd_loc;
});


