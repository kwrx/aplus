#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/attribute.h>
#include <aplus/exec.h>
#include <aplus/elf.h>

#include <stdint.h>
#include <errno.h>


extern task_t* current_task;
extern task_t* kernel_task;



char* exec_symbol_lookup(uint32_t symaddr) {
	#define ELF32_ST_BIND(i)	((i >> 4))
	#define ELF32_ST_TYPE(i)	((i) & 0x0F)
	
	const char* strtab = (const char*) current_task->symbols.strtab;
	elf32_sym_t* symtab = (elf32_sym_t*) current_task->symbols.symtab;
	uint32_t symtabsz = current_task->symbols.count;


	for(int i = 0; i < (symtabsz / sizeof(elf32_sym_t)); i++) {
		if(ELF32_ST_TYPE(symtab[i].st_info) != 0x02)
			continue;

		if((symaddr >= symtab[i].st_value) && (symaddr < (symtab[i].st_value + symtab[i].st_size))) {
			const char* name = (const char*) ((uint32_t) strtab + symtab[i].st_name);
			return (char*) name;
		}
	}


	strtab = (const char*) kernel_task->symbols.strtab;
	symtab = (elf32_sym_t*) kernel_task->symbols.symtab;
	symtabsz = kernel_task->symbols.count;

	for(int i = 0; i < (symtabsz / sizeof(elf32_sym_t)); i++) {
		if(ELF32_ST_TYPE(symtab[i].st_info) != 0x02)
			continue;

		if((symaddr >= symtab[i].st_value) && (symaddr < (symtab[i].st_value + symtab[i].st_size))) {
			const char* name = (const char*) ((uint32_t) strtab + symtab[i].st_name);
			return (char*) name;
		}
	}

	return NULL;
}



int exec_init_kernel_task(task_t* k) {
	elf32_shdr_t* shdr = (elf32_shdr_t*) mbd->exec.addr;
	if(unlikely(!shdr))
		return -1;

	uint32_t shstrtab = shdr[mbd->exec.shndx].sh_addr;

	for(int i = 0; i < mbd->exec.num; i++) {
		const char* name = (const char*) (shstrtab + shdr[i].sh_name);

		if(strcmp(name, ".strtab") == 0)
			k->symbols.strtab = (uint32_t) shdr[i].sh_addr;
		

		if(strcmp(name, ".symtab") == 0) {
			k->symbols.symtab = (uint32_t) shdr[i].sh_addr;
			k->symbols.count = shdr[i].sh_size;
		}
	}

	return 0;
}





void* exec_load(char* path, void* image, uintptr_t* vaddr, size_t* vsize) {
	list_t* ls = attribute("exec");
	if(list_empty(ls))
		return NULL;
	

	list_foreach(v, ls) {
		exec_loader_t* loader = (exec_loader_t*) v;

		if(memcmp(image, loader->magic, strlen(loader->magic)) == 0) {
#ifdef EXEC_DEBUG
			kprintf("exec: loading \"%s\" with \"%s\" loader.\n", path, loader->name);
#endif
			return loader->load(path, image, vaddr, vsize);
		}
	}

#ifdef EXEC_DEBUG
	kprintf("exec: no loader found!\n");
#endif

	errno = ENOEXEC;
	return NULL;
}




EXPORT_SYMBOL(exec_symbol_lookup);
