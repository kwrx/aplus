#ifndef _EXEC_H
#define _EXEC_H

#include <stdint.h>

typedef struct exec_loader {
	char* name;
	char* magic;
	
	void* (*load) (char* path, void* image, uintptr_t* vaddr, size_t* vsize);
} exec_loader_t;


#define EXEC(n, m, l)														\
	static exec_loader_t exec_##n = {										\
		#n, m, l															\
	}; 																		\
	ATTRIBUTE("exec", exec_##n);											\

#endif
