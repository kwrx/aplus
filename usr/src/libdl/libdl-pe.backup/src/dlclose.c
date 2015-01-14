#include <dlfcn.h>
#include "dl.h"

int dlclose(void* handle) {
	dll_t* dll = (dll_t*) handle;

	rtsym_t* rt = dll->symbols;
	while(rt) {
		rtsym_t* t = rt;
		rt = rt->next;

		__dl_free(t);
	}

	dll->symbols = 0;

	__dl_free(dll->image);
	__dl_free(dll);

	return 0;
}
