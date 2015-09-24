#include <xdev.h>
#include <xdev/binfmt.h>
#include <xdev/debug.h>
#include <xdev/mm.h>
#include <xdev/elf.h>
#include <xdev/module.h>
#include <libc.h>


extern int arch_elf_check_machine(elf_module_t* elf);
extern int arch_elf_reloc_section(elf_module_t* elf, Elf_Shdr* shdr);
extern int arch_elf_reloca_section(elf_module_t* elf, Elf_Shdr* shdr);

#define elf_sym_name(elf, offs)	\
	((char*) ((uintptr_t) (elf)->names + (offs)))

#define elf_get_ptr(elf, addr)	\
	((Elf_Addr) ((uintptr_t) (elf)->start + (addr)))

#define elf_sec_ptr(elf, shdr)	\
	((Elf_Addr) ((uintptr_t) (elf)->header + (shdr)->sh_offset))



static int elf_define(elf_module_t* elf, char* name, void* value) {
	symbol_t* sym;
	for(sym = elf->symbols; sym; sym = sym->next) {
		if(strcmp(sym->name, name) != 0)
			continue;


		kprintf(WARN, "elf: symbol %s already defined at 0x%x\n", name, sym->addr);
		sym->addr = value;
		return E_OK;
	}

	sym = (symbol_t*) kmalloc(sizeof(symbol_t) + strlen(name) + 1, GFP_KERNEL);
	memset(sym, 0, sizeof(symbol_t) + strlen(name) + 1);

	strcpy(sym->name, name);
	sym->addr = value;
	sym->next = elf->symbols;

	elf->symbols = sym;
	return E_OK;
}

static void* elf_resolve(elf_module_t* elf, char* name) {
	symbol_t* sym;
	for(sym = elf->symbols; sym; sym = sym->next)
		if(strcmp(sym->name, name) == 0)
			return sym->addr;

	kprintf(ERROR, "elf: cannot resolve \"%s\"\n", name);
	return NULL;
}

static void elf_layout(elf_module_t* elf) {
	int i;
	Elf_Shdr* shdr;

	for(i = 1; i < elf->header->e_shnum; i++) {
		shdr = &elf->sections[i];

		if(!(shdr->sh_flags & SHF_ALLOC))
			continue;

		shdr->sh_addr = shdr->sh_addralign
			? (elf->size + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1)
			: elf->size
			;

		elf->size = shdr->sh_addr + shdr->sh_size;
	}
}

static int elf_reloc(elf_module_t* elf) {
	int i;
	Elf_Shdr* shdr = &elf->sections[0];

	for(i = 0; i < elf->header->e_shnum; i++) {
		switch(shdr->sh_type) {
			case SHT_REL:
				arch_elf_reloc_section(elf, shdr);
				break;
			case SHT_RELA:
				arch_elf_reloca_section(elf, shdr);
				break;
		}

		shdr++;
	}

	return E_OK;
}

static int elf_link(elf_module_t* elf) {

	extern int export_start;
	extern int export_end;

	struct {
		char* name;
		void* address;
	} *exports = NULL;

	for(exports = (void*) &export_start; (void*) exports < (void*) &export_end; exports++)
		elf_define(elf, exports->name, exports->address);




	int err;
	int n = elf->symtab->sh_size / sizeof(Elf_Sym);

	Elf_Sym* sym;
	Elf_Sym* symtab = (Elf_Sym*) elf_sec_ptr(elf, elf->symtab);
	Elf_Sym* end = &symtab[n];

	for(sym = &symtab[1]; sym < end; sym++) {
		switch(sym->st_shndx) {
			case SHN_COMMON:
				continue;

			case SHN_ABS:
				continue;

			case SHN_UNDEF:
				sym->st_value = (Elf_Addr) elf_resolve(elf, elf_sym_name(elf, sym->st_name));
				break;
			default:
				sym->st_value += (Elf_Addr) elf_get_ptr(elf, elf->sections[sym->st_shndx].sh_addr);

				if(ELF_SYM_TYPE(sym->st_info) != STT_SECTION) {
					err = elf_define(elf, elf_sym_name(elf, sym->st_name), (void*) sym->st_value);

					if(err < 0)
						return err;
				}
		}
	}

	return E_OK;
}


static void* elf_module_load(void* image, void** address, size_t* size) {
	elf_module_t _elf;
	elf_module_t* elf = &_elf;
	
	elf->header = (Elf_Ehdr*) image;

	if(memcmp(elf->header->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1) || (arch_elf_check_machine(elf))) {
		kprintf(ERROR, "elf: invalid elf image\n");
		return NULL;
	}

	elf->sections = (void*) ((uintptr_t) image + elf->header->e_shoff);
	elf->strings = (void*) ((uintptr_t) image + elf->sections[elf->header->e_shstrndx].sh_offset);
	elf->size = 0;

	int i;
	for(i = 1; i < elf->header->e_shnum; i++) {
		Elf_Shdr* shdr = &elf->sections[i];

		if(shdr->sh_type == SHT_SYMTAB) {
			elf->symtab = &elf->sections[i];
			elf->strtab = &elf->sections[elf->sections[i].sh_link];
			elf->names = (char*) ((uintptr_t) image + elf->strtab->sh_offset);
		}
	}

	elf_layout(elf);

	void* core = (void*) kmalloc(elf->size, GFP_KERNEL);
	KASSERT(core);

	elf->start = core;
	
	Elf_Shdr* shdr;
	for(i = 1; i < elf->header->e_shnum; i++) {
		shdr = &elf->sections[i];

		if(!(shdr->sh_flags & SHF_ALLOC))
			continue;

		memcpy(
			(void*) elf_get_ptr(elf, shdr->sh_addr), 
			(void*) ((uintptr_t) elf->header + shdr->sh_offset),
			shdr->sh_size
		);
	}

	if((i = elf_link(elf)) < 0)
		return NULL;

	if((i = elf_reloc(elf)) < 0)
		return NULL;



	void* init = (void*) elf_resolve(elf, "init");
	KASSERT(init);

	void* dnit = (void*) elf_resolve(elf, "dnit");
	KASSERT(dnit);

	char** __module_name__ = (char**) elf_resolve(elf, "__module_name__");
	KASSERT(__module_name__);

	char** __module_deps__ = (char**) elf_resolve(elf, "__module_deps__");
	KASSERT(__module_deps__);

	symbol_t* sym, *s2;
	for(sym = elf->symbols; sym;) {
		s2 = sym;		
		sym = sym->next;

		kfree(s2);
	}

	module_t* mod = (module_t*) kmalloc(sizeof(module_t), GFP_KERNEL);
	mod->name = strdup(*__module_name__);
	mod->deps = strdup(*__module_deps__);
	mod->init = init;
	mod->dnit = dnit;
	mod->loaded_address = (uintptr_t) core;
	mod->loaded = 0;
	mod->next = NULL;

	if(address)
		*address = (void*) core;

	return mod;
}

static int elf_module_check(void* image) {
	elf_module_t _elf;
	elf_module_t* elf = &_elf;
	
	elf->header = (Elf_Ehdr*) image;

	if(memcmp(elf->header->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1) || (arch_elf_check_machine(elf)))
		return E_ERR;

	if(elf->header->e_type != ET_REL)
		return E_ERR;

	return E_OK;
}


void elf_module_register(void) {
	binfmt_register("ELF_MODULE", elf_module_check, elf_module_load);
}

