#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/attribute.h>
#include <aplus/exec.h>

#include <stdint.h>
#include <errno.h>



void* script_load(char* path, void* image, uintptr_t* vaddr, size_t* size) {
	char x[64], *s = (char*) image;
	memset(x, 0, 64);

	for(int i = 2, j = 0; i < *size; i++, j++) {
		if(s[i] == '\n')
			break;

		x[j] = s[i];
	}

	strcat(x, " ");
	strcat(x, path);


#ifdef EXEC_DEBUG
	kprintf("exec: loading script \"%s\"\n", x);
#endif


#if 0
	execl("/bin/sh", "-c", x, NULL);
#endif

	return NULL;
}

EXEC(Script, "#!", script_load);
