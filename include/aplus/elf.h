/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_ELF_H
#define _APLUS_ELF_H


#ifndef __ASSEMBLY__
#include <stdint.h>
#include <sys/types.h>
#include <elf.h>


#if defined(KERNEL)

typedef struct symbol {
	void* address;
	char name[1];	
} symbol_t;

#endif


#if defined(__i386__) || defined(__arm__)
#define Elf_Half 		Elf32_Half
#define Elf_Word 		Elf32_Word
#define Elf_Sword		Elf32_Sword
#define Elf_Xword		Elf32_Xword
#define Elf_Addr		Elf32_Addr
#define Elf_Off			Elf32_Off
#define Elf_Section		Elf32_Section
#define Elf_Versym		Elf32_Versym
#define Elf_Ehdr		Elf32_Ehdr
#define Elf_Shdr		Elf32_Shdr
#define Elf_Chdr		Elf32_Chdr
#define Elf_Sym 		Elf32_Sym
#define Elf_Syminfo		Elf32_Syminfo
#define Elf_Rel			Elf32_Rel
#define Elf_Rela		Elf32_Rela
#define Elf_Phdr		Elf32_Phdr
#define Elf_Dyn			Elf32_Dyn
#define Elf_Verdef		Elf32_Verdef
#define Elf_Verdaux		Elf32_Verdaux
#define Elf_Verneed		Elf32_Verneed
#define Elf_Vernaux		Elf32_Vernaux
#define Elf_auxv_t		Elf32_auxv_t
#define Elf_Nhdr		Elf32_Nhdr
#define Elf_Move		Elf32_Move
#define Elf_Lib			Elf32_Lib

#define ELF_R_SYM		ELF32_R_SYM
#define ELF_R_TYPE		ELF32_R_TYPE
#define ELF_R_INFO		ELF32_R_INFO

#define ELF_ST_BIND		ELF32_ST_BIND
#define ELF_ST_TYPE		ELF32_ST_TYPE
#define ELF_ST_INFO		ELF32_ST_INFO

#elif defined(__x86_64__) || defined(__aarch64__)
#define Elf_Half 		Elf64_Half
#define Elf_Word 		Elf64_Word
#define Elf_Sword		Elf64_Sword
#define Elf_Xword		Elf64_Xword
#define Elf_Addr		Elf64_Addr
#define Elf_Off			Elf64_Off
#define Elf_Section		Elf64_Section
#define Elf_Versym		Elf64_Versym
#define Elf_Ehdr		Elf64_Ehdr
#define Elf_Shdr		Elf64_Shdr
#define Elf_Chdr		Elf64_Chdr
#define Elf_Sym 		Elf64_Sym
#define Elf_Syminfo		Elf64_Syminfo
#define Elf_Rel			Elf64_Rel
#define Elf_Rela		Elf64_Rela
#define Elf_Phdr		Elf64_Phdr
#define Elf_Dyn			Elf64_Dyn
#define Elf_Verdef		Elf64_Verdef
#define Elf_Verdaux		Elf64_Verdaux
#define Elf_Verneed		Elf64_Verneed
#define Elf_Vernaux		Elf64_Vernaux
#define Elf_auxv_t		Elf64_auxv_t
#define Elf_Nhdr		Elf64_Nhdr
#define Elf_Move		Elf64_Move
#define Elf_Lib			Elf64_Lib

#define ELF_R_SYM		ELF64_R_SYM
#define ELF_R_TYPE		ELF64_R_TYPE
#define ELF_R_INFO		ELF64_R_INFO

#define ELF_ST_BIND		ELF64_ST_BIND
#define ELF_ST_TYPE		ELF64_ST_TYPE
#define ELF_ST_INFO		ELF64_ST_INFO

#else
#error "Unknown platform, define CONFIG_BITS = (32|64)"
#endif


#endif
#endif