#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/task.h>
#include <aplus/elf.h>
#include <aplus/base.h>
#include <aplus/utils/list.h>
#include <libc.h>

extern int arch_elf_check_machine(Elf_Ehdr* elf);

static list(module_t*, m_queue);
static list(symbol_t*, m_symtab);


static void module_export(module_t* mod, char* name, void* address) {

	if(mod) {
		int def = 0;

		if(strcmp(name, "init") == 0)
			mod->init = address;
		else if(strcmp(name, "dnit") == 0)
			mod->dnit = address;
		else if(strcmp(name, "__module_name__") == 0)
			mod->name = (char*) address;
		else if(strcmp(name, "__module_deps__") == 0)
			mod->deps = (char*) address;
		else
			def++;

		if(!def)
			return;
	}
		

	list_each(m_symtab, v) {
		if(strcmp(v->name, name) == 0) {
			kprintf(WARN "module: \'%s\' already defined at %p\n", name, v->address);
			return;
		}
	}

	symbol_t* s = (symbol_t*) kmalloc(sizeof(symbol_t) + strlen(name) + 1, GFP_KERNEL);
	if(!s) {
		kprintf(ERROR "module: could not allocate memory for a new export!\n");
		return;
	}

	s->address = address;
	strcpy(s->name, name);

	list_push(m_symtab, s);
}


static void* module_resolve(char* name) {
	list_each(m_symtab, v)
		if(strcmp(v->name, name) == 0)
			return v->address;
	
	kprintf(ERROR "module: undefined reference for \'%s\'\n", name);
	return NULL;
}


static int module_run(char* name) {
	list_each(m_queue, mod) {
		if(strcmp(mod->name, name) != 0)
			continue;

		if(mod->loaded)
			return E_OK;

		if(mod->deps && strlen(mod->deps) > 0) {
			list(char*, l_deps);
			char* deps = strdup(mod->deps);

			char* p;
			for(p = strtok(deps, ","); p; p = strtok(NULL, ","))
				list_push(l_deps, p);

			list_each(l_deps, v) {
				if(module_run(v) != E_OK) {
					kprintf(ERROR "module: unresolved dependency \'%s\' for %s\n", v, mod->name);
					return E_ERR;
				}
			}

			list_clear(l_deps);
			kfree(deps);
		}

		if(!mod->init) {
			kprintf(ERROR, "module: unresolved entrypoint \'init()\' for %s\n", mod->name);
			return E_ERR;
		}


		kprintf(INFO "module: running %s (%s)\n", mod->name, mod->deps);
		mod->init();
		mod->loaded++;

		return E_OK;
	}

	return E_ERR;
}


int module_init(void) {

	extern int export_start;
	extern int export_end;

	struct {
		char* name;
		void* address;
	} *exports = NULL;

	for(exports = (void*) &export_start; (void*) exports < (void*) &export_end; exports++)
		module_export(NULL, exports->name, exports->address);



	int i;
	for(i = 0; i < mbd->modules.count; i++) {
		
		size_t size = (size_t) mbd->modules.ptr[i].size;
		void* image = (void*) mbd->modules.ptr[i].ptr;

		module_t* mod = (module_t*) kmalloc(sizeof(module_t), GFP_KERNEL);
		memset(mod, 0, sizeof(module_t));

		Elf_Ehdr* hdr = (Elf_Ehdr*) image;
		if(memcmp(hdr->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1) || (arch_elf_check_machine(hdr)) || (hdr->e_type != ET_REL)) {
			kprintf(ERROR "module[%d]: invalid executable!\n", i);
			continue;
		}


		Elf_Shdr* shdr = (Elf_Shdr*) ((uintptr_t) image + hdr->e_shoff);
		Elf_Shdr* symtab = NULL;
		Elf_Shdr* strtab = NULL;
		char* names = NULL;

		for(int i = 1; i < hdr->e_shnum; i++) {
			if(shdr[i].sh_type == SHT_SYMTAB) {
				symtab = &shdr[i];
				strtab = &shdr[shdr[i].sh_link];
				names = (char*) ((uintptr_t) image + strtab->sh_offset);
			}

			if(!(shdr[i].sh_flags & SHF_ALLOC))
				continue;

			shdr[i].sh_addr = shdr[i].sh_addralign
				? (size + shdr[i].sh_addralign - 1) & ~(shdr[i].sh_addralign - 1)
				: (size)
				;

			size = shdr[i].sh_addr + shdr[i].sh_size;
		}

		if(!symtab || !strtab || !names) {
			kprintf(ERROR "module[%d]: could not found symtab, strtab or names\n", i);
			continue;
		}


		void* core = (void*) kmalloc(size, GFP_KERNEL);
		if(!core) {
			kprintf(ERROR "module[%d]: could not allocate %d bytes of memory!\n", i, size);
			continue;
		}


		for(int i = 1; i < hdr->e_shnum; i++) {
			if(!(shdr[i].sh_flags & SHF_ALLOC))
				continue;

			memcpy (
				(void*) ((uintptr_t) core + shdr[i].sh_addr),
				(void*) ((uintptr_t) image + shdr[i].sh_offset),
				shdr[i].sh_size
			);
		}

		Elf_Sym* sym = (Elf_Sym*) ((uintptr_t) image + symtab->sh_offset);
		for(int i = 1; i < symtab->sh_size / sizeof(Elf_Sym); i++) {
			switch(sym[i].st_shndx) {
				case SHN_COMMON:
					continue;
				
				case SHN_ABS:
					continue;

				case SHN_UNDEF:
					sym[i].st_value = (Elf_Addr) module_resolve((char*) ((uintptr_t) names + sym[i].st_name));
					break;
				
				default:
					sym[i].st_value += (Elf_Addr) ((uintptr_t) core + shdr[sym[i].st_shndx].sh_addr);

					if(ELF_SYM_TYPE(sym[i].st_info) != STT_SECTION)
						module_export(mod, (char*) ((uintptr_t) names + sym[i].st_name), (void*) sym[i].st_value);

					break;
			}
		}

		for(int i = 0; i < hdr->e_shnum; i++) {
			if(shdr[i].sh_type != SHT_REL)
				continue;

			Elf_Rel* rel = (Elf_Rel*) ((uintptr_t) image + shdr[i].sh_offset);
			Elf_Sym* stab = (Elf_Sym*) ((uintptr_t) image + symtab->sh_offset);

			for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rel); j++) {
				Elf_Addr* ptr = (Elf_Addr*) ((uintptr_t) core + shdr[shdr[i].sh_info].sh_addr + rel[j].r_offset);
				Elf_Sym* sym = &stab[ELF_REL_SYM(rel[j].r_info)];

				switch(ELF_REL_TYPE(rel[j].r_info)) {
					case R_386_32:
						*ptr += sym->st_value;
						break;
					case R_386_PC32:
						*ptr += sym->st_value - (Elf_Addr) ptr;
						break;
					default:
						continue;
				}
			}
		}



		mod->name = *((char**) mod->name);
		mod->deps = *((char**) mod->deps);
		mod->loaded_address = (uintptr_t) core;
		mod->loaded = 0;

		list_push(m_queue, mod);
	}


	list_each(m_queue, mod)
		module_run(mod->name);

	return E_OK;
}
