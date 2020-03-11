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
#include <aplus/memory.h>
#include <aplus/utils/list.h>

#include <string.h>

#include <hal/cpu.h>
#include <hal/vmm.h>
#include <hal/timer.h>


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
        

    list_each(m_symtab, v) {
        
        if(strcmp(v->name, name) == 0)
            kpanicf("module: PANIC! \'%s\' already defined at %p (%s)\n", name, v->address, mod ? mod->name : "undefined");
    
    }
    

    symbol_t* s = (symbol_t*) kmalloc(sizeof(symbol_t) + strlen(name) + 1, GFP_KERNEL);

    s->address = address;
    strcpy(s->name, name);

    list_push(m_symtab, s);

}



static void* module_resolve(module_t* m, const char* name) {

    list_each(m_symtab, v)
        if(strcmp(v->name, name) == 0)
            return v->address;


    void* p;
    if((p = (void*) runtime_get_address(name)))
        return module_export(NULL, name, p)
             , p;
    
    
    kpanicf("module: PANIC! undefined reference for \'%s\' in %s\n", name, m->name);
    return NULL;
}




void module_run(module_t* m) {

    if(m->status == MODULE_STATUS_LOADED)
        return;

    if(m->status == MODULE_STATUS_LOADING)
        kpanicf("module: PANIC! dependency loop for '%s'\n", m->name);


    m->status = MODULE_STATUS_LOADING;



#if defined(DEBUG) && DEBUG_LEVEL >= 2
    kprintf("module: loading %s [addr(%p), size(%p)]\n", m->name, m->core.ptr, m->core.size);
#endif



    #define find(s) ({                                                  \
                                                                        \
        module_t* r = NULL;                                             \
        list_each(m_queue, m) {                                         \
            if(strcmp(m->name, s) != 0)                                 \
                continue;                                               \
                                                                        \
            r = m;                                                      \
            break;                                                      \
        }                                                               \
                                                                        \
        if(!r)                                                          \
            kpanicf("module: PANIC! unresolved dependency' %s'\n", s);  \
        r;                                                              \
    })


    do {

        if(m->deps[0] == '\0')
            break;


        if(strchr(m->deps, ',') == NULL)
            module_run(find(m->deps));

        else {

            int i = 0;
            for(char* s = strchr(m->deps, ','); s; s = strchr(++s, ','))
                i++;

            module_t* deps[i];


            i = 0;
            for(char* s = strtok((char*) m->deps, ","); s; s = strtok(NULL, ","))
                deps[i++] = find(s);

            while(i)
                module_run(deps[--i]);
            
        }

    } while(0);




    int i;
    for(i = 0; i < m->exe.symtab->sh_size / m->exe.symtab->sh_entsize; i++) {

        #define syname(p) \
            ((const char*) ((uintptr_t) m->exe.header + m->exe.strtab->sh_offset + p))


        Elf_Sym* s = &((Elf_Sym*) ((uintptr_t) m->exe.header + m->exe.symtab->sh_offset)) [i];

        switch(s->st_shndx) {

            case SHN_ABS:
                break;

            case SHN_UNDEF:
                s->st_value = (Elf_Addr) module_resolve(m, syname(s->st_name));
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



    /* SHT_REL */
    for(int i = 1; i < m->exe.header->e_shnum; i++) {

        if(m->exe.section[i].sh_type != SHT_REL)
            continue;

        Elf_Rel* r = (Elf_Rel*) ((uintptr_t) m->exe.header + m->exe.section[i].sh_offset);
        Elf_Sym* s = (Elf_Sym*) ((uintptr_t) m->exe.header + m->exe.section[m->exe.section[i].sh_link].sh_offset);
    

        int j;
        for(j = 0; j < m->exe.section[i].sh_size / m->exe.section[i].sh_entsize; j++) {

            Elf_Addr* obj = (Elf_Addr*) ((uintptr_t) m->core.ptr + m->exe.section[m->exe.section[i].sh_info].sh_addr + r[j].r_offset);
            Elf_Sym* sym = &s[ELF_R_SYM(r[j].r_info)];


            #define A ((Elf_Addr) *obj)
            #define P ((Elf_Addr) obj)
            #define S ((Elf_Addr) sym->st_value)
            #define B ((Elf_Addr) m->core.ptr)
            
            #define _(x, y) \
                case x: { *obj = y; } break;



            switch(ELF_R_TYPE(r[j].r_info)) {

 #if defined(__i386__)

                case R_386_NONE:
                case R_386_COPY:
                    break;

                _(R_386_32,         S + A       )
                _(R_386_PC32,       S + A - P   )
                _(R_386_GLOB_DAT,   S           )
                _(R_386_JMP_SLOT,   S           )            
                _(R_386_RELATIVE,   B + A       )
/*
                _(R_386_GOT32,      G + A - P   )
                _(R_386_PLT32,      L + A - P   )
                _(R_386_GOTOFF,     S + A - GOT )
                _(R_386_GOTPC,      GOT + A - P )
                _(R_386_SIZE32,     Z + A       )
*/
#endif

#if defined(__x86_64__)

                case R_X86_64_NONE:
                case R_X86_64_COPY:
                    break;

                _(R_X86_64_64,          S + A       )
                _(R_X86_64_PC32,        S + A - P   )
                _(R_X86_64_GLOB_DAT,    S           )
                _(R_X86_64_JUMP_SLOT,   S           )             
                _(R_X86_64_RELATIVE,    B + A       )
                _(R_X86_64_32,          S + A       )
                _(R_X86_64_32S,         S + A       )
                _(R_X86_64_16,          S + A       )
                _(R_X86_64_PC16,        S + A - P   )
                _(R_X86_64_8,           S + A       )
                _(R_X86_64_PC8,         S + A - P   )
                _(R_X86_64_PC64,        S + A - P   )
/*
                _(R_X86_64_GOT32,       G + A       )
                _(R_X86_64_PLT32,       L + A - P   )
                _(R_X86_64_GOTOFF64,    S + A - GOT )
                _(R_X86_64_GOTPC32,     GOT + A + P )             
                _(R_X86_64_SIZE32,      Z + A       )             
                _(R_X86_64_SIZE64,      Z + A       )             
                _(R_X86_64_GOTPCREL,    G + GOT + A - P)
*/
#endif

                default:
                    kpanicf("module: PANIC! unknown relocation SHT_REL type %d for %s\n", ELF_R_TYPE(r[j].r_info), m->name);
                    break;

            }

            #undef A
            #undef B
            #undef P
            #undef S
            #undef _
        }
    }



    /* SHT_RELA */
    for(int i = 1; i < m->exe.header->e_shnum; i++) {

        if(m->exe.section[i].sh_type != SHT_RELA)
            continue;

        Elf_Rela* r = (Elf_Rela*) ((uintptr_t) m->exe.header + m->exe.section[i].sh_offset);
        Elf_Sym* s = (Elf_Sym*) ((uintptr_t) m->exe.header + m->exe.section[m->exe.section[i].sh_link].sh_offset);
    

        int j;
        for(j = 0; j < m->exe.section[i].sh_size / m->exe.section[i].sh_entsize; j++) {

            Elf_Addr* obj = (Elf_Addr*) ((uintptr_t) m->core.ptr + m->exe.section[m->exe.section[i].sh_info].sh_addr + r[j].r_offset);
            Elf_Sym* sym = &s[ELF_R_SYM(r[j].r_info)];


            #define A ((Elf_Addr) r[j].r_addend)
            #define P ((Elf_Addr) obj)
            #define S ((Elf_Addr) sym->st_value)
            #define B ((Elf_Addr) m->core.ptr)
            
            #define _(x, y) \
                case x: { *obj = y; } break;



            switch(ELF_R_TYPE(r[j].r_info)) {

 #if defined(__i386__)

                case R_386_NONE:
                case R_386_COPY:
                    break;

                _(R_386_32,         S + A       )
                _(R_386_PC32,       S + A - P   )
                _(R_386_GLOB_DAT,   S           )
                _(R_386_JMP_SLOT,   S           )            
                _(R_386_RELATIVE,   B + A       )
/*
                _(R_386_GOT32,      G + A - P   )
                _(R_386_PLT32,      L + A - P   )
                _(R_386_GOTOFF,     S + A - GOT )
                _(R_386_GOTPC,      GOT + A - P )
                _(R_386_SIZE32,     Z + A       )
*/
#endif

#if defined(__x86_64__)

                case R_X86_64_NONE:
                case R_X86_64_COPY:
                    break;

                _(R_X86_64_64,          S + A       )
                _(R_X86_64_PC32,        S + A - P   )
                _(R_X86_64_GLOB_DAT,    S           )
                _(R_X86_64_JUMP_SLOT,   S           )             
                _(R_X86_64_RELATIVE,    B + A       )
                _(R_X86_64_32,          S + A       )
                _(R_X86_64_32S,         S + A       )
                _(R_X86_64_16,          S + A       )
                _(R_X86_64_PC16,        S + A - P   )
                _(R_X86_64_8,           S + A       )
                _(R_X86_64_PC8,         S + A - P   )
                _(R_X86_64_PC64,        S + A - P   )
/*
                _(R_X86_64_GOT32,       G + A       )
                _(R_X86_64_PLT32,       L + A - P   )
                _(R_X86_64_GOTOFF64,    S + A - GOT )
                _(R_X86_64_GOTPC32,     GOT + A + P )             
                _(R_X86_64_SIZE32,      Z + A       )             
                _(R_X86_64_SIZE64,      Z + A       )             
                _(R_X86_64_GOTPCREL,    G + GOT + A - P)
*/
#endif

                default:
                    kpanicf("module: PANIC! unknown relocation SHT_RELA type %d for %s\n", ELF_R_TYPE(r[j].r_info), m->name);
                    break;

            }

            #undef A
            #undef B
            #undef P
            #undef S
            #undef _
        }
    }


#if defined(DEBUG) && DEBUG_LEVEL >= 1
    kprintf("module: running %s [init(%p) args(%p)]\n", m->name, m->init, m->args);
#endif

    m->init(m->args);
    m->status = MODULE_STATUS_LOADED;

}




void module_init(void) {

    memset(&m_queue, 0, sizeof(m_queue));
    memset(&m_symtab, 0, sizeof(m_symtab));


    int i;
    for(i = 0; i < core->modules.count; i++) {

        module_t* m = (module_t*) kcalloc(1, sizeof(module_t), GFP_KERNEL);


        m->exe.header = (Elf_Ehdr*) arch_vmm_p2v(core->modules.ko[i].ptr, ARCH_VMM_AREA_HEAP);
        
        DEBUG_ASSERT(m->exe.header);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG0] == ELFMAG0);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG1] == ELFMAG1);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG2] == ELFMAG2);
        DEBUG_ASSERT(m->exe.header->e_ident[EI_MAG3] == ELFMAG3);
        DEBUG_ASSERT(m->exe.header->e_type == ET_REL);


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

            DEBUG_ASSERT(m->exe.section[i].sh_type != SHT_DYNAMIC);
            DEBUG_ASSERT(m->exe.section[i].sh_type != SHT_DYNSYM);

            
            m->exe.section[j].sh_addr = m->exe.section[j].sh_addralign
                ? (m->core.size + m->exe.section[j].sh_addralign - 1) & ~(m->exe.section[j].sh_addralign - 1)
                : (m->core.size)
                ;


            m->core.size = m->exe.section[j].sh_addr + m->exe.section[j].sh_size;

        }


        m->core.ptr = (void*) kmalloc(m->core.size, GFP_KERNEL);
        

        for(int j = 1; j < m->exe.header->e_shnum; j++) {
            
            if(m->exe.section[j].sh_type == SHT_SYMTAB) {
                m->exe.symtab = &m->exe.section[j];
                m->exe.strtab = &m->exe.section[m->exe.section[j].sh_link];
            }

    
            if(!(m->exe.section[j].sh_flags & SHF_ALLOC))
                continue;


            switch(m->exe.section[j].sh_type) {
                
                case SHT_PROGBITS:

                    memcpy (
                        (void*) ((uintptr_t) m->core.ptr + m->exe.section[j].sh_addr),
                        (void*) ((uintptr_t) m->exe.header + m->exe.section[j].sh_offset),
                        m->exe.section[j].sh_size
                    );
                    
                    break;

                case SHT_NOBITS:

                    memset (
                        (void*) ((uintptr_t) m->core.ptr + m->exe.section[j].sh_addr),
                        0,
                        m->exe.section[j].sh_size
                    );

                    break;

                default:
                    kpanicf("module: PANIC! invalid section type for %s: %d\n", m->name, m->exe.section[j].sh_type);

            }

        }

        DEBUG_ASSERT(m->exe.symtab);
        DEBUG_ASSERT(m->exe.strtab);


        m->status = MODULE_STATUS_READY;
        m->refcount = 1;
        m->args = (const char*) &core->modules.ko[i].cmdline;


        list_each(m_queue, v)
            if(strcmp(m->name, v->name) == 0)
                kpanicf("module: PANIC! duplicate name '%s'\n", m->name);

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