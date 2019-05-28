/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_MODULE_H
#define _APLUS_MODULE_H

#include <aplus.h>
#include <aplus/elf.h>


#define MODULE_NAME(x)                                  \
    __attribute__((section(".module_name")))            \
    const char __module_name__[] = x "\0"

#define MODULE_DEPS(x)                                  \
    __attribute__((section(".module_deps")))            \
    const char __module_deps__[] = x "\0"


#define MODULE_AUTHOR(x);
#define MODULE_LICENSE(x);


#define MODULE_STATUS_UNKNOWN                           0
#define MODULE_STATUS_READY                             1
#define MODULE_STATUS_LOADED                            2
#define MODULE_STATUS_LOADING                           3
#define MODULE_STATUS_FAILED                            4


typedef struct {
    struct {
        Elf_Ehdr* header;
        Elf_Shdr* section;
        Elf_Shdr* symtab;
        Elf_Shdr* strtab;
        Elf_Shdr* shstrtab;
    } exe;

    struct {
        void* ptr;
        size_t size;
    } core;


    void (*init) (const char*);
    void (*dnit) ();

    const char* name;
    const char* deps;
    const char* args;

    int status;
    int refcount;
} module_t;


void module_init(void);
#endif
