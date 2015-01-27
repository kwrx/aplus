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


	sym = (symbol_t*) malloc(sizeof(*sym) + strlen(name) + 1);
	strcpy(sym->name, name);
	sym->addr = addr;
	sym->next = elf->symbols;

	return 0;
}


static void* resolve_symbol(elf_module_t* elf, char* name) {
	symbol_t* sym = elf->symbols;
	symbol_t* fnd = NULL;	
	while(sym) {
		if(strcmp(sym->name, name) == 0) {
			fnd = sym;
			break;
		}

		sym = sym->next;
	}

	if(fnd == NULL)
		return NULL;

	return sym->addr;
}


int elf_load_module(elf_module_t* elf, void* image, int size, char* start) {
	elf_module_link_cbs_t lnk = {
		resolve_symbol,
		define_symbol
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
