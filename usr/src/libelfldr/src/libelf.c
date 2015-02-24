#include <string.h>
#include <errno.h>

#include <aplus/elf.h>

static int define_symbol(elf_module_t* elf, char* name, void* addr) {
	symbol_t* sym = elf->symbols;
	while(sym) {
		if(strcmp(sym->name, name) == 0) {
			errno = EINVAL;
			return -1;
		}

		sym = sym->next;
	}

	sym = (symbol_t*) malloc(sizeof(symbol_t) + strlen(name) + 1);
	strcpy(sym->name, name);
	sym->addr = addr;
	sym->next = elf->symbols;
	elf->symbols = sym;


	return 0;
}


static void* resolve_symbol(elf_module_t* elf, char* name) {
	symbol_t* sym = elf->symbols;	
	while(sym) {
		if(strcmp(sym->name, name) == 0)
			return sym->addr;

		sym = sym->next;
	}


	printf("elf: could not resolve \"%s\"\n", name);

	return NULL;
}


int elf_load_module(elf_module_t* elf, void* image, int size, char* start) {

	memset(elf, 0, sizeof(elf_module_t));

	elf_module_link_cbs_t lnk = {
		.resolve = resolve_symbol,
		.define = define_symbol
	};


	if(elf_module_init(elf, image, size) < 0) {
		errno = ENOEXEC;
		return -1;
	}
	

	void* core = (void*) malloc(size);
	if(!core) {
		errno = ENOMEM;
		return -1;
	}


	if(elf_module_load(elf, core, &lnk) < 0) {
		errno = ENOEXEC;
		return -1;
	}


	elf->start = resolve_symbol(elf, start);
	free(image);

	return 0;
}
