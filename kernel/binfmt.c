#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/binfmt.h>
#include <libc.h>

static binfmt_t* binfmt_queue = NULL;

int binfmt_check_image(void* image, const char* loader) {
	binfmt_t* tmp;
	for(tmp = binfmt_queue; tmp; tmp = tmp->next) {
		if(loader)	
			if(strcmp(tmp->name, loader) != 0)
				continue;

		if(tmp->check(image) == E_OK)
			return E_OK;

	}


	return E_ERR;
}

void* binfmt_load_image(void* image, void** address, size_t* size, const char* loader) {
	
	binfmt_t* tmp;
	for(tmp = binfmt_queue; tmp; tmp = tmp->next) {
		if(loader)	
			if(strcmp(tmp->name, loader) != 0)
				continue;

		if(tmp->check(image) == E_OK)
			return tmp->load(image, address, size);

	}

	kprintf(ERROR, "binfmt: invalid executable format!\n");

	return NULL;
}

int binfmt_register(const char* name, int (*check) (void*), void* (*load) (void*, void**, size_t*)) {
	binfmt_t* fmt = (binfmt_t*) kmalloc(sizeof(binfmt_t), GFP_KERNEL);
	fmt->name = strdup(name);
	fmt->check = check;
	fmt->load = load;
	fmt->next = binfmt_queue;
	binfmt_queue = fmt;

	return E_OK;
}




EXPORT(binfmt_load_image);
EXPORT(binfmt_register);
