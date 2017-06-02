#ifndef _BINFMT_H
#define _BINFMT_H

#include <aplus.h>
#include <libc.h>


typedef struct symbol {
	struct symbol* next;

	void* addr;
	char name[1];	
} symbol_t;

typedef struct binfmt {
	const char* name;
	
	int (*check)(void* image);
	void* (*load)(void* image, void** address, symbol_t** symtab, size_t* size);

	struct binfmt* next;
} binfmt_t;




int binfmt_register(const char* name, int (*check) (void*), void* (*load) (void*, void**, symbol_t**, size_t*));
void* binfmt_load_image(void* image, void** address, symbol_t** symtab, size_t* size, const char* loader);
char* binfmt_check_image(void* image, const char* loader);
char* binfmt_lookup_symbol(symbol_t* symtab, uintptr_t address);

#endif
