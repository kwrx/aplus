#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/binfmt.h>
#include <libc.h>

static binfmt_t* binfmt_queue = NULL;

char* binfmt_check_image(void* image, const char* loader) {
	binfmt_t* tmp;
	for(tmp = binfmt_queue; tmp; tmp = tmp->next) {
		if(loader)	
			if(strcmp(tmp->name, loader) != 0)
				continue;

		if(tmp->check(image) == E_OK)
			return (char*) tmp->name;

	}

	return NULL;
}

void* binfmt_load_image(void* image, void** address, symbol_t** symtab, size_t* size, const char* loader) {
	
	binfmt_t* tmp;
	for(tmp = binfmt_queue; tmp; tmp = tmp->next) {
		if(loader) {
			if(strcmp(tmp->name, loader) != 0)
				continue;
		} else
			if(tmp->check(image) != E_OK)
				continue;

		return tmp->load(image, address, symtab, size);
	}

	kprintf(ERROR "binfmt: invalid executable format!\n");
	return NULL;
}

int binfmt_register(const char* name, int (*check) (void*), void* (*load) (void*, void**, symbol_t**, size_t*)) {
	binfmt_t* fmt = (binfmt_t*) kmalloc(sizeof(binfmt_t), GFP_KERNEL);
	fmt->name = strdup(name);
	fmt->check = check;
	fmt->load = load;
	fmt->next = binfmt_queue;
	binfmt_queue = fmt;

	return E_OK;
}


char* binfmt_lookup_symbol(symbol_t* symtab, uintptr_t address) {
	symbol_t* tmp, *found = NULL;
	for(tmp = symtab; tmp; tmp = tmp->next) {
		if((uintptr_t) tmp->addr == address) {
			found = tmp;
			break;
		}

		if((uintptr_t) tmp->addr < address)
			if(!found || found->addr < tmp->addr)
				found = tmp;
	}
	

	if(!found)
		return NULL;
		
	static char buf[BUFSIZ];
	memset(buf, 0, BUFSIZ);

	sprintf(buf, "%s+%p", found->name, (void*) (address - (uintptr_t) found->addr));
	return strdup(buf);
}



EXPORT(binfmt_load_image);
EXPORT(binfmt_register);
