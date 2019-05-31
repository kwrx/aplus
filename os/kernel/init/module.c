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
#include <aplus/module.h>
#include <aplus/elf.h>
#include <aplus/mm.h>
#include <aplus/utils/list.h>

#include <string.h>


extern int elf_check_machine(Elf_Ehdr* elf);

list(module_t*, m_queue);
list(symbol_t*, m_symtab);


static void module_export(module_t* mod, const char* name, void* address) {
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(address);


    if(likely(mod)) {
        do {
            if(strcmp(name, "init") == 0)
                mod->init = address;
            else if(strcmp(name, "dnit") == 0)
                mod->dnit = address;
            else if(strcmp(name, "__module_name__") == 0)
                ;
            else if(strcmp(name, "__module_deps__") == 0)
                ;
            else
                break;

            return;
        } while(0);
    }
        

    list_each(m_symtab, v)
        if(strcmp(v->name, name) == 0)
            kpanic("module: \'%s\' already defined at %p", name, v->address);
    

    symbol_t* s = (symbol_t*) kmalloc(sizeof(symbol_t) + strlen(name) + 1, GFP_KERNEL);
    DEBUG_ASSERT(s);

    s->address = address;
    strcpy(s->name, name);

    list_push(m_symtab, s);
}



static void* module_resolve(const char* name) {
    list_each(m_symtab, v)
        if(strcmp(v->name, name) == 0)
            return v->address;
    
    kpanic("module: undefined reference for \'%s\'", name);
}



void module_run(module_t* m) {
    
    if(m->status == MODULE_STATUS_LOADED)
        return;

    if(m->status == MODULE_STATUS_LOADING)
        kpanic("module: dependency loop for '%s'", m->name);


    m->status = MODULE_STATUS_LOADING;

    for(char* p = strtok((char*) m->deps, ","); p; strtok(NULL, ",")) {
        list_each(m_queue, d)
            if(strcmp(d->name, p) == 0)
                module_run(d);
    }



    int i;
    for(i = 1; i < m->exe.symtab->sh_size / sizeof(Elf_Sym); i++) {

        #define syname(p) \
            ((const char*) ((uintptr_t) m->exe.header + m->exe.strtab->sh_offset + p))


        Elf_Sym* s = &((Elf_Sym*) ((uintptr_t) m->exe.header + m->exe.symtab->sh_offset)) [i];

        switch(s->st_shndx) {
            case SHN_COMMON:
                break;

            case SHN_ABS:
                break;

            case SHN_UNDEF:
                s->st_value = (Elf_Addr) module_resolve(syname(s->st_name));
                break;

            default:
                s->st_value += (Elf_Addr) ((uintptr_t) m->core.ptr + m->exe.section[s->st_shndx].sh_addr);
        
                if(ELF_ST_TYPE(s->st_info) == STT_FUNC)
                    if(ELF_ST_BIND(s->st_info) == STB_GLOBAL)
                        module_export(m, syname(s->st_name), (void*) s->st_value);
        
                break;
        }


        #undef syname
    }


    DEBUG_ASSERT(m->init);
    DEBUG_ASSERT(m->dnit);


    for(int i = 0; i < m->exe.header->e_shnum; i++) {

        if(m->exe.section[i].sh_type != SHT_REL)
            continue;

        Elf_Rel* r = (Elf_Rel*) ((uintptr_t) m->exe.header + m->exe.section[i].sh_offset);
        Elf_Sym* s = (Elf_Sym*) ((uintptr_t) m->exe.header + m->exe.symtab->sh_offset);
    
        int j;
        for(j = 0; j < m->exe.section[i].sh_size / sizeof(Elf_Sym); j++) {

            Elf_Addr* obj = (Elf_Addr*) ((uintptr_t) m->core.ptr + m->exe.section[m->exe.section[i].sh_info].sh_addr + r[j].r_offset);
            Elf_Sym* sym = &s[ELF_R_SYM(r[j].r_info)];


            #define A ((Elf_Addr) *obj)
            #define P ((Elf_Addr) obj)
            #define S ((Elf_Addr) sym->st_value)
            #define B ((Elf_Addr) m->core.ptr)

            switch(ELF_R_TYPE(r[j].r_info)) {
                
                case R_386_32:
                    *obj = S + A;
                    break;

                case R_386_PC32:
                    *obj = S + A - P;
                    break;

#if 0
                case R_386_GOT32:
                    *obj = G + A - P;
                    break;
                
                case R_386_PLT32:
                    *obj = L + A - P;
                    break;
#endif

                case R_386_COPY:
                    break;

                case R_386_GLOB_DAT:
                case R_386_JMP_SLOT:
                    *obj = S;
                    break;             

                case R_386_RELATIVE:
                    *obj = B + A;
                    break;

#if 0
                case R_386_GOTOFF:
                    *obj = S + A - GOT;
                    break;

                case R_386_GOTPC:
                    *obj = GOT + A - P;
                    break;
#endif

                default:
                    kpanic("module: invalid relocation type %d for %s", ELF_R_TYPE(r[j].r_info), m->name);
                    break;

            }

            #undef A
            #undef B
            #undef P
            #undef S
        }
    }


#if DEBUG
    kprintf("module: running %s\n", m->name);
#endif

    m->init(m->args);
    m->status = MODULE_STATUS_LOADED;
}




void module_init(void) {
    memset(&m_queue, 0, sizeof(m_queue));
    memset(&m_symtab, 0, sizeof(m_symtab));


    extern int export_start;
    extern int export_end;

    struct {
        char* name;
        void* address;
    } *exports = NULL;

    for(exports = (void*) &export_start; (void*) exports < (void*) &export_end; exports++)
        module_export(NULL, exports->name, exports->address);



    int i;
    for(i = 0; i < mbd->modules.count; i++) {

        module_t* m = (module_t*) kcalloc(sizeof(module_t), 1, GFP_KERNEL);
        DEBUG_ASSERT(m);


        m->exe.header = (Elf_Ehdr*) mbd->modules.ptr[i].ptr;
        
        DEBUG_ASSERT(m->exe.header);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG0] == ELFMAG0);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG1] == ELFMAG1);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG2] == ELFMAG2);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG3] == ELFMAG3);
        //DEBUG_ASSERT(elf_check_machine(m->exe.header) == 0);


        m->exe.section = (Elf_Shdr*) ((uintptr_t) m->exe.header + m->exe.header->e_shoff);
        m->exe.shstrtab = &m->exe.section[m->exe.header->e_shstrndx];

        DEBUG_ASSERT(m->exe.section);
        DEBUG_ASSERT(m->exe.shstrtab);


        int j;
        for(j = 1; j < m->exe.header->e_shnum; j++) {

            #define syname(p) \
                ((const char*) ((uintptr_t) m->exe.header + m->exe.shstrtab->sh_offset + p))


            if(strcmp(syname(m->exe.section[j].sh_name), ".module_name") == 0)
                m->name = (const char*) ((uintptr_t) m->exe.header + m->exe.section[j].sh_offset);

            if(strcmp(syname(m->exe.section[j].sh_name), ".module_deps") == 0)
                m->deps = (const char*) ((uintptr_t) m->exe.header + m->exe.section[j].sh_offset);


            #undef syname           
        }

        DEBUG_ASSERT(m->name);
        DEBUG_ASSERT(m->deps);


        for(int j = 1; j < m->exe.header->e_shnum; j++) {

            if(!(m->exe.section[j].sh_flags & SHF_ALLOC))
                continue;

            
            m->exe.section[j].sh_addr = m->exe.section[j].sh_addralign
                ? (m->core.size + m->exe.section[j].sh_addralign - 1) & ~(m->exe.section[j].sh_addralign - 1)
                : (m->core.size)
                ;

            m->core.size = m->exe.section[j].sh_addr + m->exe.section[j].sh_size;

        }


        m->core.ptr = (void*) kmalloc(m->core.size, GFP_KERNEL);
        DEBUG_ASSERT(m->core.ptr);
        


        for(int j = 1; j < m->exe.header->e_shnum; j++) {
            
            if(m->exe.section[j].sh_type == SHT_SYMTAB) {
                m->exe.symtab = &m->exe.section[j];
                m->exe.strtab = &m->exe.section[m->exe.section[j].sh_link];
            }

            if(!(m->exe.section[j].sh_flags & SHF_ALLOC))
                continue;

            
            memcpy (
                (void*) ((uintptr_t) m->core.ptr + m->exe.section[j].sh_addr),
                (void*) ((uintptr_t) m->exe.header + m->exe.section[j].sh_offset),
                m->exe.section[j].sh_size
            );

        }

        DEBUG_ASSERT(m->exe.symtab);
        DEBUG_ASSERT(m->exe.strtab);


        m->status = MODULE_STATUS_READY;
        m->refcount = 1;
        m->args = (const char*) mbd->modules.ptr[i].cmdline;


        list_each(m_queue, v)
            if(strcmp(m->name, v->name) == 0)
                kpanic("module: duplicate name '%s'", m->name);

        list_push(m_queue, m);
    }


    list_each(m_queue, m)
        module_run(m);

}


void module_dnit(void) {
  
    list_each(m_queue, mod)
        if(mod->status == MODULE_STATUS_LOADED)
            mod->dnit();

}