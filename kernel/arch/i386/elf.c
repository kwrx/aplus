#include <aplus.h>
#include <aplus/elf.h>
#include <aplus/debug.h>
#include <libc.h>
#include <arch/i386/i386.h>


int arch_elf_check_machine(Elf_Ehdr* elf) {
	if((elf->e_machine == EM_386) || (elf->e_machine == EM_486))
		return E_OK;

	return E_ERR;
}


EXPORT(arch_elf_check_machine);
