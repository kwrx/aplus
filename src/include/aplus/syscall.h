#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <aplus.h>
#include <aplus/attribute.h>

#include <stdint.h>
#include <errno.h>



typedef struct __syscall {
	void* handler;
	int number;
} syscall_t;


#define SYSCALL(h, n)														\
	static syscall_t syscall_##n = {										\
		(void*) h, (int) n													\
	}; 																		\
	ATTRIBUTE("syscall", syscall_##n)


#endif
