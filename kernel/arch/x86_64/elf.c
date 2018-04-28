#include <aplus.h>
#include <aplus/elf.h>
#include <aplus/debug.h>
#include <libc.h>
#include <arch/x86_64/x86_64.h>


int elf_check_machine(Elf_Ehdr* elf) {
    if(elf->e_machine == EM_X86_64)
        return 0;

    return -1;
}


EXPORT(elf_check_machine);
