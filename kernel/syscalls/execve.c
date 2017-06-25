#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/binfmt.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/intr.h>
#include <aplus/mm.h>
#include <libc.h>


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


extern char** args_dup(char**);

SYSCALL(2, execve,
int sys_execve(const char* filename, char* const argv[], char* const envp[]) {
#if CONFIG_CACHE
	static int e_initialized = 0;
	static kcache_pool_t e_pool;

	if(unlikely(!e_initialized)) {
		e_initialized++;

		kcache_register_pool(&e_pool);
	}
#endif


	int fd = sys_open(filename, O_RDONLY, 0);
	if(fd < 0)
		return -1;


	inode_t* inode = current_task->fd[fd].inode;


	int r;
	if(inode->uid == current_task->uid)
		r = __check_perm(0, inode->mode);
	else if(inode->gid == current_task->gid)
		r = __check_perm(1, inode->mode);
	else
		r = __check_perm(2, inode->mode);


	if(unlikely(!r)) {
		sys_close(fd);

		errno = EACCES;
		return -1;
	}


	size_t size = sys_lseek(fd, 0, SEEK_END);
	sys_lseek(fd, 0, SEEK_SET);


	void* image = (void*) kmalloc(size + 1, GFP_USER);
	if(unlikely(!image)) {
		sys_close(fd);
		return -1;
	}

	((char*) image) [size] = 0;

#if CONFIG_CACHE
	void* cache = NULL;
	if(kcache_obtain_block(&e_pool, inode->ino, &cache, size) != 0) {
		if(unlikely(!cache))
			cache = image;
#else
	void* cache = image;
#endif

		if(unlikely(sys_read(fd, cache, size) != size)) { /* ERROR */
			kfree(image);
			
			errno = EIO;
			return -1;
		}

#if CONFIG_CACHE
	}
	
	if(likely(cache != image))
		memcpy(image, cache, size);
		
	kcache_release_block(&e_pool, inode->ino);
#endif

	sys_close(fd);

	char* loader;
	if(!(loader = binfmt_check_image(image, NULL))) {
		kfree(image);

		errno = ENOEXEC;
		return -1;
	}
	

	char** __new_argv = args_dup((char**) argv);
	char** __new_envp = args_dup((char**) envp);
	
	filename = strdup(filename);



	INTR_OFF;
	arch_task_release(current_task);


	void (*_start) (char**, char**) = (void (*) (char**, char**)) binfmt_load_image(image, (void**) &current_task->image->start, &current_task->image->symtab, &size, loader);
	KASSERT(_start);

	
	kfree(image);

	

	current_task->argv = __new_argv;
	current_task->environ = __new_envp;
	current_task->image->end = ((current_task->image->start + size + PAGE_SIZE) & ~(PAGE_SIZE - 1)) + 0x10000;
	current_task->name = filename;
	current_task->description = "";
	current_task->exe = inode;


	INTR_ON;
	syscall_ack();

	_start((char**) current_task->argv, (char**) current_task->environ);
	KASSERT(0);
	
	return -1;
});
