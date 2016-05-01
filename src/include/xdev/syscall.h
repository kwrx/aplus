#ifndef _SYSCALL_H
#define _SYSCALL_H

int syscall_init(void);
int syscall_register(int, void*);
int syscall_unregister(int);


long syscall_handler(long, long, long, long, long, long);



#define SYSCALL(x, y, z)			\
	z					\
	__attribute__((section(".syscalls")))	\
	struct {				\
		int a;				\
		void* b;			\
		char* name;			\
	} __sc_##y = {				\
		(int) x,			\
		(void*) sys_##y,		\
		(char*) #y			\
	}; EXPORT(sys_##y)	


#endif
