#ifndef _XDEV_H
#define _XDEV_H



#define E_ERR		(-1)
#define E_OK		(0)

#ifndef __ASSEMBLY__

#include <libc.h>


typedef struct bootargs {
	struct {
		uint64_t size;
		uint32_t pagesize;
		uintptr_t start;
	} memory;

	struct {
		struct {
			uintptr_t ptr;
			size_t size;
			uintptr_t cmdline;
			uintptr_t reserved;
		} __packed *ptr;
		size_t count;
	} modules;

	struct {
		uint16_t width;
		uint16_t height;
		uint16_t depth;
		uint32_t pitch;
		uintptr_t base;
		uint32_t size;
	} lfb;

	struct {
		char* args;
		int length;
	} cmdline;

	struct {
		uint32_t num;
		uintptr_t addr;
		uint32_t size;
		uint32_t shndx;
	} exec;

	int flags;
} bootargs_t;



#define EXPORT(f)			\
	__section(".exports")	\
	struct {				\
		char* name;			\
		void* addr;			\
	} __export_##f = {		\
		(char*) #f,			\
		(void*) &f			\
	};



extern bootargs_t* mbd;


extern int sys_mount(const char* dev, const char* dir, const char* fstype, unsigned long int options, const void* data);
extern int sys_chown(const char*, uid_t, gid_t);
extern int sys_chroot(const char*);
extern int sys_clone(int (*)(void*), void*, int, void*);
extern int sys_close(int);
extern void sys_exit(int);
extern int sys_fork(void);
extern int sys_fstat(int, struct stat*);
extern int sys_getdents(int, struct dirent*, size_t);
extern pid_t sys_getpid(void);
extern int isatty(int);
extern int sys_kill(pid_t, int);
extern int sys_link(const char*, const char*);
extern off_t sys_lseek(int, off_t, int);
extern int sys_open(const char*, int, mode_t);
extern int sys_read(int, void*, size_t);
extern void* sys_sbrk(ptrdiff_t);
extern clock_t sys_times(struct tms*);
extern int sys_wait(int*);
extern int sys_waitpid(pid_t, int*, int);
extern int sys_write(int, void*, size_t);
extern void sys_yield(void);
extern void* sys_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off);
extern void* sys_munmap(void* addr, size_t len);
extern int sys_execve(const char* filename, char* const argv[], char* const envp[]);


extern void arch_loop_idle();

#endif

#endif
