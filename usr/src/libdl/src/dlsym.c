#include <dlfcn.h>
#include "config.h"
#include "dl.h"

void* dlsym(void* handle, const char* name) {
	dll_t* dll = (dll_t*) handle;

	rtsym_t* rt = dll->symbols;
	while(rt) {
		if(dl_strcmp(rt->name, name) == 0)
			return rt->addr;

		rt = rt->next;
	}

	return NULL;
}
