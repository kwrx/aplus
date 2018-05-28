/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#ifndef _MODULE_H
#define _MODULE_H

#include <aplus/base.h>
#include <aplus/elf.h>
#include <aplus/utils/list.h>

#define MODULE_NAME(x)                                  \
    __attribute__((section(".module_name")))            \
    const char __module_name__[] = x "\0"

#define MODULE_DEPS(x)                                  \
    __attribute__((section(".module_deps")))            \
    const char __module_deps__[] = x "\0"


#define MODULE_AUTHOR(x);
#define MODULE_LICENSE(x);



typedef struct module {
    char* name;
    list(char*, deps);

    int (*init) (void);
    int (*dnit) (void);
    
    uintptr_t loaded_address;
    uintptr_t image_address;

    size_t size;
    int loaded;
    int refcount;
} module_t;


int module_init(void);
int module_dnit(void);

int module_check(void* image, size_t size, char** name);
int module_run(char* name);
int module_load(char* name);
int module_exit(char* name);


extern list(module_t*, m_queue);
extern list(symbol_t*, m_symtab);


#define __APLUS_MODULE__        1
#endif
