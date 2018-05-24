#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/task.h>
#include <aplus/elf.h>
#include <aplus/base.h>
#include <aplus/utils/list.h>
#include <aplus/utils/hashmap.h>
#include <libc.h>


extern int elf_check_machine(Elf_Ehdr* elf);

list(module_t*, m_queue);
list(symbol_t*, m_symtab);


static void module_export(module_t* mod, char* name, void* address) {

    if(mod) {
        int def = 0;

        if(strcmp(name, "init") == 0)
            mod->init = address;
        else if(strcmp(name, "dnit") == 0)
            mod->dnit = address;
        else if(strcmp(name, "__module_name__") == 0)
            ;
        else if(strcmp(name, "__module_deps__") == 0)
            ;
        else
            def++;

        if(!def)
            return;
    }
        

    list_each(m_symtab, v) {
        if(strcmp(v->name, name) == 0) {
            kprintf(WARN "module: \'%s\' already defined at %p\n", name, v->address);
            return;
        }
    }

    symbol_t* s = (symbol_t*) kmalloc(sizeof(symbol_t) + strlen(name) + 1, GFP_KERNEL);
    if(!s) {
        kprintf(ERROR "module: could not allocate memory for a new export!\n");
        return;
    }

    s->address = address;
    strcpy(s->name, name);

    list_push(m_symtab, s);
}


static void* module_resolve(char* name) {
    list_each(m_symtab, v)
        if(strcmp(v->name, name) == 0)
            return v->address;
    
    kprintf(ERROR "module: undefined reference for \'%s\'\n", name);
    return NULL;
}


int module_load(char* name) {
    list_each(m_queue, mod) {
        if(strcmp(mod->name, name) != 0)
            continue;

        if(mod->loaded_address)
            return 0;

        list_each(mod->deps, d) {
            if(module_load(d) != 0) {
                kprintf(ERROR "module: unresolved dependency on loading \'%s\' for %s\n", d, mod->name);
                errno = EBADMSG;
                return -1;
            }
        }



        void* image = (void*) mod->image_address;
        Elf_Ehdr* hdr = (Elf_Ehdr*) image;
        Elf_Shdr* shdr = (Elf_Shdr*) ((uintptr_t) image + hdr->e_shoff);
        Elf_Shdr* symtab = NULL;
        Elf_Shdr* strtab = NULL;
        char* names = NULL;

        size_t size = 0;
        for(int i = 1; i < hdr->e_shnum; i++) {
            if(shdr[i].sh_type == SHT_SYMTAB) {
                symtab = &shdr[i];
                strtab = &shdr[shdr[i].sh_link];
                names = (char*) ((uintptr_t) image + strtab->sh_offset);
            }

            if(!(shdr[i].sh_flags & SHF_ALLOC))
                continue;

            shdr[i].sh_addr = shdr[i].sh_addralign
                ? (size + shdr[i].sh_addralign - 1) & ~(shdr[i].sh_addralign - 1)
                : (size)
                ;

            size = shdr[i].sh_addr + shdr[i].sh_size;
        }

        void* core = (void*) kmalloc(size, GFP_KERNEL);
        if(!core) {
            kprintf(ERROR "module: could not allocate %d bytes of memory for %s!\n", size, name);
            errno = ENOMEM;
            return -1;
        }

        for(int i = 1; i < hdr->e_shnum; i++) {
            if(shdr[i].sh_type == SHT_SYMTAB) {
                symtab = &shdr[i];
                strtab = &shdr[shdr[i].sh_link];
                names = (char*) ((uintptr_t) image + strtab->sh_offset);
            }

            if(!(shdr[i].sh_flags & SHF_ALLOC))
                continue;

            memcpy (
                (void*) ((uintptr_t) core + shdr[i].sh_addr),
                (void*) ((uintptr_t) image + shdr[i].sh_offset),
                shdr[i].sh_size
            );
        }


        if(!symtab || !strtab || !names) {
            kprintf(ERROR "module: could not found symtab, strtab or names for %s\n", name);
            errno = EBADMSG;
            return -1;
        }

        Elf_Sym* sym = (Elf_Sym*) ((uintptr_t) image + symtab->sh_offset);
        for(int i = 1; i < symtab->sh_size / sizeof(Elf_Sym); i++) {
            switch(sym[i].st_shndx) {
                case SHN_COMMON:
                    continue;
                
                case SHN_ABS:
                    continue;

                case SHN_UNDEF:
                    sym[i].st_value = (Elf_Addr) module_resolve((char*) ((uintptr_t) names + sym[i].st_name));

                    break;
                
                default:
                    sym[i].st_value += (Elf_Addr) ((uintptr_t) core + shdr[sym[i].st_shndx].sh_addr);

                    if(ELF_SYM_TYPE(sym[i].st_info) != STT_SECTION)
                        module_export(mod, (char*) ((uintptr_t) names + sym[i].st_name), (void*) sym[i].st_value);

                    break;
            }
        }

        
        
        for(int i = 0; i < hdr->e_shnum; i++) {
            if(shdr[i].sh_type != SHT_REL)
                continue;

            Elf_Rel* rel = (Elf_Rel*) ((uintptr_t) image + shdr[i].sh_offset);
            Elf_Sym* stab = (Elf_Sym*) ((uintptr_t) image + symtab->sh_offset);

            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rel); j++) {
                Elf_Addr* ptr = (Elf_Addr*) ((uintptr_t) core + shdr[shdr[i].sh_info].sh_addr + rel[j].r_offset);
                Elf_Sym* sym = &stab[ELF_REL_SYM(rel[j].r_info)];

                switch(ELF_REL_TYPE(rel[j].r_info)) {
                    case R_386_32:
                        *ptr += sym->st_value;
                        break;
                    case R_386_PC32:
                        *ptr += sym->st_value - (Elf_Addr) ptr;
                        break;
                    default:
                        continue;
                }
            }
        }




        mod->loaded_address = (uintptr_t) core;
        return 0;
    }

    errno = ENOENT;
    return -1;
}

int module_run(char* name) {
    list_each(m_queue, mod) {
        if(strcmp(mod->name, name) != 0)
            continue;

        mod->refcount++;
        if(mod->loaded)
            return 0;

        list_each(mod->deps, d) {
            if(module_run(d) != 0) {
                kprintf(ERROR "module: unresolved dependency on running \'%s\' for %s\n", d, mod->name);
                errno = EBADMSG;
                return -1;
            }
        }


        if(!mod->init) {
            kprintf(ERROR, "module: unresolved entrypoint \'init()\' for %s\n", mod->name);
            errno = EBADMSG;
            return -1;
        }

        if(mod->init() != 0)
            return -1;

        mod->loaded++;
        return 0;
    }

    return -1;
}


int module_check(void* image, size_t size, char** retname) {
    Elf_Ehdr* hdr = (Elf_Ehdr*) image;
    if(memcmp(hdr->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1) || (elf_check_machine(hdr)) || (hdr->e_type != ET_REL)) {
        errno = ENOEXEC;
        return -1;
    }

    Elf_Shdr* shdr = (Elf_Shdr*) ((uintptr_t) image + hdr->e_shoff);
    char* shstr = (char*) ((uintptr_t) image + shdr[hdr->e_shstrndx].sh_offset);
    
    
    char* deps = NULL;
    char* name = NULL;

    for(int i = 1; i < hdr->e_shnum; i++) {
        if(strcmp((char*) ((uintptr_t) shstr + shdr[i].sh_name), ".module_name") == 0)
            name = strdup((char*) ((uintptr_t) image + shdr[i].sh_offset));

        if(strcmp((char*) ((uintptr_t) shstr + shdr[i].sh_name), ".module_deps") == 0)
            deps = strdup((char*) ((uintptr_t) image + shdr[i].sh_offset));
    }


    module_t* mod = (module_t*) kmalloc(sizeof(module_t), GFP_KERNEL);
    memset(mod, 0, sizeof(module_t));

    list_each(m_queue, m) {
        if(strcmp(m->name, name) == 0) {
            errno = EEXIST;
            return -1;
        }
    }
    

    if(deps && strlen(deps))
        for(char* p = strtok(deps, ","); p; p = strtok(NULL, ","))
            list_push(mod->deps, p);


    mod->name = name;
    mod->image_address = (uintptr_t) image;
    mod->size = size;
    mod->loaded_address = 0;
    mod->loaded = 0;
    mod->refcount = 0;

    if(retname)
        *retname = mod->name;

    list_push(m_queue, mod);
    return 0;
}


int module_exit(char* name) {
    list_each(m_queue, mod) {
        if(strcmp(mod->name, name) != 0)
            continue;

        if(!mod->loaded) {
            errno = EBUSY;
            return -1;
        }

        if(mod->refcount > 1) {
            errno = EWOULDBLOCK;
            return -1;
        }

        if(mod->dnit)
            mod->dnit();

        

        kfree((void*) mod->name);
        kfree((void*) mod->loaded_address);
        kfree((void*) mod);
        return 0;
    }

    errno = ENOENT;
    return -1;
}


int module_init(void) {
    memset(&m_queue, 0, sizeof(m_queue));
    memset(&m_symtab, 0, sizeof(m_queue));


    extern int export_start;
    extern int export_end;

    struct {
        char* name;
        void* address;
    } *exports = NULL;

    for(exports = (void*) &export_start; (void*) exports < (void*) &export_end; exports++)
        module_export(NULL, exports->name, exports->address);


    int i;
    for(i = 0; i < mbd->modules.count; i++)
        module_check((void*) mbd->modules.ptr[i].ptr, mbd->modules.ptr[i].size, NULL);
    
    list_each(m_queue, mod)
        module_load(mod->name);
    
    list_each(m_queue, mod)
        module_run(mod->name);


    return 0;
}


int module_dnit(void) {
    list_each(m_queue, mod) {
        if(!mod->loaded)
            continue;

        if(!mod->dnit)
            continue;

        mod->dnit();
    }

    return 0;
}


EXPORT(module_init);
EXPORT(module_dnit);
EXPORT(m_queue);
EXPORT(m_symtab);