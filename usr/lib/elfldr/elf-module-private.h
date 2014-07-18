#ifndef _ELF_MODULE_PRIVATE_H
#define _ELF_MODULE_PRIVATE_H

#include <stdint.h>

#include "elf-i386.h"
#include "elf-common.h"

#if ELF_BITS == 32
#include "elf-32.h"
#else
#include "elf-64.h"
#endif

#include "elf-module.h"

/*
 * Check compatibility with binary's target machine.
 */
extern int elf_module_check_machine(elf_module_t *elf);

/*
 * Relocate section of ELF file to new location, specified by `start' field.
 */
extern int elf_module_reloc_section(elf_module_t *elf, Elf_Shdr *shdr);

/*
 * Relocate with addition section of ELF file to new location,
 * specified by `start' field.
 */
extern int elf_module_reloca_section(elf_module_t *elf, Elf_Shdr *shdr);

#endif /* _ELF_MODULE_PRIVATE_H */
