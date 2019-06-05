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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/elf.h>
#include <aplus/ipc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>


uintptr_t core_get_address(const char* name) {
    
    Elf_Shdr* strtab = NULL;
    Elf_Shdr* symtab = NULL;
    Elf_Shdr* shdr = (Elf_Shdr*) mbd->exec.addr;
    
    #define shname(name) \
        ((const char*) (shdr[mbd->exec.shndx].sh_addr + CONFIG_KERNEL_BASE + name))

    #define syname(name) \
        ((const char*) (strtab->sh_addr + CONFIG_KERNEL_BASE + name))

    #define sypobj(index) \
        ((Elf_Sym*) (symtab->sh_addr + CONFIG_KERNEL_BASE + symtab->sh_entsize * index))
    


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


    for(j = 1; j < (symtab->sh_size / symtab->sh_entsize); j++)
        if(strcmp(syname(sypobj(j)->st_name), name) == 0)
            return sypobj(j)->st_value;


    return 0;  
}

const char* core_get_name(uintptr_t address) {

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


    const char* s = NULL;
    for(j = 1; j < (symtab->sh_size / symtab->sh_entsize); j++) {

        if(!in(address, sypobj(j)->st_value, sypobj(j)->st_size))
            continue;

        s = syname(sypobj(j)->st_name);
        break;
    }

    return s;
}
