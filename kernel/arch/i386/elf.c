#include <aplus.h>
#include <aplus/elf.h>
#include <aplus/debug.h>
#include <libc.h>
#include <arch/i386/i386.h>


int elf_check_machine(Elf_Ehdr* elf) {
    if((elf->e_machine == EM_386) || (elf->e_machine == EM_486))
        return 0;

    return -1;
}


EXPORT(elf_check_machine);
