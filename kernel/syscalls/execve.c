#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/intr.h>
#include <aplus/mm.h>
#include <aplus/elf.h>
#include <libc.h>


extern int arch_elf_check_machine(Elf_Ehdr* elf);
extern char* strndup(const char*, size_t);

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

	
	if(strncmp((const char*) image, "#!", 2) == 0) {
		char* p;
		if((p = strchr((const char*) image, '\n')))
			p = strndup(image, (uintptr_t) p - (uintptr_t) image);
		else
			p = strdup((char*) image);

		p++;
		p++;


		char* argv[32];
		memset(argv, 0, sizeof(argv));

		int i = 0;
		for(char* s = strtok((char*) p, " "); s; s = strtok(NULL, " "))
			argv[i++] = s;
		argv[i++] = NULL;



		if((p = strchr((const char*) image, '\n'))) {
			size -= (uintptr_t) ++p - (uintptr_t) image;

			int fd = sys_open(tmpnam(NULL), O_CREAT | O_RDWR, S_IFREG | 0666);
			if(fd < 0) {
				kfree(image);
				return -1;
			}

			sys_fcntl(fd, F_DUPFD, STDIN_FILENO);
			sys_write(STDIN_FILENO, p, size);
			sys_lseek(STDIN_FILENO, 0, SEEK_SET);
			sys_close(fd);
		}

		return sys_execve(argv[0], argv, current_task->environ);
	}
	


	Elf_Ehdr* hdr = (Elf_Ehdr*) image;
	if ((memcmp(hdr->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1)) 	||
		(arch_elf_check_machine(hdr)) 								||
		(hdr->e_type != ET_EXEC)) {

		kfree(image);

		errno = ENOEXEC;
		return -1;
	}




	char** __new_argv = args_dup((char**) argv);
	char** __new_envp = args_dup((char**) envp);
	
	filename = strdup(filename);



	INTR_OFF;
	arch_task_release(current_task);

	
	Elf_Shdr* shdr = (Elf_Shdr*) ((uintptr_t) image + hdr->e_shoff);
	for(int i = 1; i < hdr->e_shnum; i++) {
		if(!(shdr[i].sh_flags & SHF_ALLOC))
			continue;

		shdr[i].sh_addr = shdr->sh_addralign
			? (size + shdr[i].sh_addralign - 1) & ~(shdr[i].sh_addralign - 1)
			: (size)
			;

		size = shdr[i].sh_addr + shdr[i].sh_size;
	}

	Elf_Phdr* phdr = (Elf_Phdr*) ((uintptr_t) image + hdr->e_phoff);
	for(int i = 0; i < hdr->e_phnum; i++) {
		switch(phdr[i].p_type) {
			case 0:
				continue;
			case 1:
				if(unlikely(!sys_mmap((void*) phdr[i].p_vaddr, (phdr[i].p_memsz + phdr[i].p_align - 1) & ~(phdr[i].p_align - 1), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_ANON, -1, 0))) {
					kprintf(ERROR "elf: invalid mapping 0x%x (%d Bytes)\n", phdr[i].p_vaddr, phdr[i].p_memsz);
					kfree(image);
					
					errno = EFAULT;
					return -1;
				}

				memcpy (
					(void*) phdr[i].p_vaddr,
					(void*) ((uintptr_t) image + phdr[i].p_offset),
					phdr[i].p_filesz
				);
				break;
		}
	}




	void (*_start) (char**, char**) = (void (*) (char**, char**)) hdr->e_entry;
	KASSERT(_start);
	kfree(image);



	current_task->argv = __new_argv;
	current_task->environ = __new_envp;
	current_task->image->start = (uintptr_t) _start;
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
