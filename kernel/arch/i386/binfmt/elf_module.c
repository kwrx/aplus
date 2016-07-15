#include <xdev.h>
#include <xdev/elf.h>
#include <xdev/debug.h>
#include <libc.h>
#include <arch/i386/i386.h>


int arch_elf_check_machine(elf_module_t* elf) {

	if((elf->header->e_machine == EM_386) || (elf->header->e_machine == EM_486))
		return E_OK;

	return E_ERR;
}

int arch_elf_reloc_section(elf_module_t* elf, Elf_Shdr* shdr) {
	int n = shdr->sh_size / sizeof(Elf_Rel);
	
	Elf_Rel* rel = (Elf_Rel*) ((uintptr_t) elf->header + shdr->sh_offset);
	Elf_Rel* end = &rel[n];
	Elf_Sym* symtab = (Elf_Sym*) ((uintptr_t) elf->header + elf->symtab->sh_offset);

	for(; rel < end; rel++) {
		Elf_Addr* ptr = (Elf_Addr*) ((uintptr_t) elf->start + elf->sections[shdr->sh_info].sh_addr + rel->r_offset);
		Elf_Sym* sym = &symtab[ELF_REL_SYM(rel->r_info)];

		switch(ELF_REL_TYPE(rel->r_info)) {
			case R_386_32:
				*ptr += sym->st_value;
				break;
			case R_386_PC32:
				*ptr += sym->st_value - (Elf_Addr) ptr;
				break;

			default:
				return -EME_UNSUPPORTED;
		}
	}

	return E_OK;
}

int arch_elf_reloca_section(elf_module_t* elf, Elf_Shdr* shdr) {
	return -EME_UNSUPPORTED;
}

EXPORT(arch_elf_check_machine);
EXPORT(arch_elf_reloc_section);
EXPORT(arch_elf_reloca_section);
