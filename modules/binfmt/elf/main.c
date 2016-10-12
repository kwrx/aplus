#include <xdev.h>
#include <xdev/binfmt.h>
#include <xdev/debug.h>
#include <xdev/mm.h>
#include <xdev/elf.h>
#include <xdev/module.h>
#include <libc.h>


MODULE_NAME("binfmt/elf");
MODULE_DEPS("");
MODULE_AUTHOR("Antonio Natale");
MODULE_LICENSE("GPL");


extern int arch_elf_check_machine(elf_module_t* elf);
extern int arch_elf_reloc_section(elf_module_t* elf, Elf_Shdr* shdr);
extern int arch_elf_reloca_section(elf_module_t* elf, Elf_Shdr* shdr);

#define elf_sym_name(elf, offs)	\
	((char*) ((uintptr_t) (elf)->names + (offs)))

#define elf_get_ptr(elf, addr)	\
	((Elf_Addr) ((uintptr_t) (elf)->start + (addr)))

#define elf_sec_ptr(elf, shdr)	\
	((Elf_Addr) ((uintptr_t) (elf)->header + (shdr)->sh_offset))


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

static void* elf_load(void* image, void** address, size_t* size) {
	elf_module_t _elf;
	elf_module_t* elf = &_elf;
	
	elf->header = (Elf_Ehdr*) image;

	if(memcmp(elf->header->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1) || (arch_elf_check_machine(elf))) {
		kprintf(ERROR, "elf: invalid elf image\n");
		return NULL;
	}
	

	elf->sections = (void*) ((uintptr_t) image + elf->header->e_shoff);
	elf->programs = (void*) ((uintptr_t) image + elf->header->e_phoff);
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
	elf->start = (void*) elf->header->e_entry;
	

	Elf_Phdr* phdr;
	for(i = 0; i < elf->header->e_phnum; i++) {
		phdr = &elf->programs[i];

		switch(phdr->p_type) {
			case 0:
				continue;
			case 1: // LOAD
				if(unlikely(!sys_mmap((void*) phdr->p_vaddr, (phdr->p_memsz + phdr->p_align - 1) & ~(phdr->p_align - 1), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_ANON, -1, 0))) {
					kprintf(ERROR, "elf: invalid mapping 0x%x (%d Bytes)\n", phdr->p_vaddr, phdr->p_memsz);
					return NULL;
				}

				memcpy (
					(void*) phdr->p_vaddr,
					(void*) ((uintptr_t) elf->header + phdr->p_offset),
					phdr->p_filesz
				);
		}
	}



	/*Elf_Shdr* shdr;
	for(i = 1; i < elf->header->e_shnum; i++) {
		shdr = &elf->sections[i];

		if(!(shdr->sh_flags & SHF_ALLOC))
			continue;
			
		if(unlikely(!sys_mmap((void*) shdr->sh_addr, shdr->sh_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_ANON, -1, 0))) {
				kprintf(ERROR, "elf: invalid mapping 0x%x (%d Bytes)\n", shdr->sh_addr, shdr->sh_size);
				return NULL;
		}

		memcpy (
			(void*) shdr->sh_addr,
			(void*) ((uintptr_t) elf->header + shdr->sh_offset),
			shdr->sh_size
		);
	
		if(shdr->sh_type == SHT_NOBITS)
			memset(
				(void*) shdr->sh_addr,
				0,
				shdr->sh_size
			);
	}*/



	if(address)
		*address = (void*) elf->start;

	if(size)
		*size = (size_t) elf->size;

	return (void*) elf->start;
}



static int elf_check(void* image) {
	elf_module_t _elf;
	elf_module_t* elf = &_elf;
	
	elf->header = (Elf_Ehdr*) image;

	if(memcmp(elf->header->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1) || (arch_elf_check_machine(elf)))
		return E_ERR;

	if(elf->header->e_type != ET_EXEC)
		return E_ERR;

	return E_OK;
}


int init(void) {
	binfmt_register("ELF", elf_check, elf_load);

	return E_OK;
}



int dnit(void) {
	return E_OK;
}
