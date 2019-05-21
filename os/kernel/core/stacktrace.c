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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/elf.h>
#include <aplus/ipc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>



void core_stacktrace(void) {

    uintptr_t frames[10] = { 0 };
    arch_debug_stacktrace((uintptr_t*) &frames, sizeof(frames) / sizeof(uintptr_t));


    Elf_Shdr* strtab = NULL;
    Elf_Shdr* symtab = NULL;
    Elf_Shdr* shdr = (Elf_Shdr*) mbd->exec.addr;
    
    #define shname(name) \
        ((const char*) (shdr[mbd->exec.shndx].sh_addr + CONFIG_KERNEL_BASE + name))

    #define syname(name) \
        ((const char*) (strtab->sh_addr + CONFIG_KERNEL_BASE + name))

    #define sypobj(index) \
        ((Elf_Sym*) (symtab->sh_addr + CONFIG_KERNEL_BASE + symtab->sh_entsize * index))
    
    #define in(x, a, b) \
        (x >= a && x < (a + b))



    int j;
    for(j = 1; j < mbd->exec.num; j++) {

        switch(shdr[j].sh_type) {

            case SHT_STRTAB:
                if(strcmp(shname(shdr[j].sh_name), ".strtab") == 0)
                    strtab = &shdr[j];

                break;
            
            case SHT_SYMTAB:
                if(strcmp(shname(shdr[j].sh_name), ".symtab") == 0)
                    symtab = &shdr[j];

                break;

        }
    }

    DEBUG_ASSERT(strtab);
    DEBUG_ASSERT(symtab);


    int i;
    for(i = 1; i < sizeof(frames) / sizeof(uintptr_t); i++) {
        if(!frames[i])
            break;

        const char* s = NULL;
        for(j = 1; j < (symtab->sh_size / symtab->sh_entsize); j++) {

            if(ELF_ST_TYPE(sypobj(j)->st_info) != STT_FUNC)
                continue;

            if(!in(frames[i], sypobj(j)->st_value, sypobj(j)->st_size))
                continue;

            s = syname(sypobj(j)->st_name);
            break;
        }

        kprintf("[%d] %8p <%s>\n", i, frames[i], s ? s : "unknown");
    }
}