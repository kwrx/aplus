#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/binfmt.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/network.h>
#include <xdev/intr.h>
#include <xdev/mm.h>
#include <libc.h>

#define CHECKPERM 0
#if CHECKPERM
static int __check_perm(int type, mode_t mode) {
	switch(type) {
		case 0: /* UID */
			return (mode & S_IXUSR);
		case 1: /* GID */
			return (mode & S_IXGRP);
		case 2: /* OTH */
			return (mode & S_IXOTH);
		default:
			break;
	}
	
	return 0;
}
#endif

SYSCALL(2, execve,
int sys_execve(const char* filename, char* const argv[], char* const envp[]) {
	int fd = sys_open(filename, O_RDONLY, 0);
	if(fd < 0)
		return -1;


#if CHECKPERM
	int r;
	inode_t* inode = current_task->fd[fd].inode;
	if(inode->uid == current_task->uid)
		r = __check_perm(0, inode->mode);
	else if(inode->gid == current_task->gid)
		r = __check_perm(1, inode->mode);
	else
		r = __check_perm(2, inode->mode);


	if(unlikely(!r)) {
		errno = EACCES;
		return -1;
	}
#endif

	size_t size = sys_lseek(fd, 0, SEEK_END);
	sys_lseek(fd, 0, SEEK_SET);


	void* image = (void*) kmalloc(size, GFP_USER);
	if(unlikely(sys_read(fd, image, size) != size)) { /* ERROR */
		kfree(image);
		
		errno = EIO;
		return -1;
	}

	sys_close(fd);


	if(binfmt_check_image(image, NULL) != E_OK) {
		kfree(image);

		errno = ENOEXEC;
		return -1;
	}


	INTR_OFF;
	arch_task_release(current_task);

	int i;
	for(i = 0; i < TASK_FD_COUNT; i++)
		sys_close(i);


	void (*_start) (char**, char**) = (void (*) (char**, char**)) binfmt_load_image(image, (void**) &current_task->image.start, &size, NULL);
	KASSERT(_start);

	current_task->image.end = ((current_task->image.start + size + PAGE_SIZE) & ~(PAGE_SIZE - 1)) + 0x10000;
	current_task->name = strdup(filename);
	current_task->argv = (char**) argv;
	current_task->environ = (char**) envp;

	INTR_ON;

	_start((char**) argv, (char**) envp);
	KASSERT(0);
});
