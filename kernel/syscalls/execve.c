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


static char** args_dup(char** args) {
	int len = 0;
	char** ret = NULL;
	
	if(unlikely(!args)) {
		ret = (char**) kmalloc(sizeof(char**), GFP_USER);
		ret[0] = NULL;
		
		return ret;	
	}
	
	while(args[len])
		len++;

	ret = (char**) kmalloc(sizeof(char**) * (len + 1), GFP_USER);
	
	int i;
	for(i = 0; i < len; i++)
		ret[i] = strdup(args[i]);
		
	ret[len] = NULL;
	return ret;
}

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
	

	char** __new_argv = args_dup((char**) argv);
	char** __new_envp = args_dup((char**) envp);
	
	

	INTR_OFF;
	arch_task_release(current_task);


	void (*_start) (char**, char**) = (void (*) (char**, char**)) binfmt_load_image(image, (void**) &current_task->image.start, &size, NULL);
	KASSERT(_start);
	
	kfree(image);
	

	current_task->argv = __new_argv;
	current_task->environ = __new_envp;
	current_task->image.end = ((current_task->image.start + size + PAGE_SIZE) & ~(PAGE_SIZE - 1)) + 0x10000;
	current_task->name = strdup(filename);

	INTR_ON;

	_start((char**) current_task->argv, (char**) current_task->environ);
	KASSERT(0);
	
	return -1;
});
