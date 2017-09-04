#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dl.h"

#define elf_sym_name(elf, offs)     \
    ((char*) ((uintptr_t) (elf)->names + (offs)))

#define elf_get_ptr(elf, addr)      \
    ((Elf_Addr) ((uintptr_t) (elf)->start + (addr)))

#define elf_sec_ptr(elf, shdr)      \
    ((Elf_Addr) ((uintptr_t) (elf)->header + (shdr)->sh_offset))


static inline void dl_elf_layout(dl_t* dl) {
    int i;
    Elf_Shdr* shdr;

    for(i = 1; i < dl->header->e_shnum; i++) {
        shdr = &dl->sections[i];

        if(!(shdr->sh_flags & SHF_ALLOC))
            continue;

        shdr->sh_addr = shdr->sh_addralign
            ? (dl->size + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1)
            : dl->size
            ;

        dl->size = shdr->sh_addr + shdr->sh_size;
    }
}

static inline int dl_elf_reloc(dl_t* dl) {
    int i;
    Elf_Shdr* shdr = &dl->sections[0];

    for(i = 0; i < dl->header->e_shnum; i++) {
        switch(shdr->sh_type) {
            case SHT_REL: {
#if defined(__i386__) || defined(__arm__)
                int n = shdr->sh_size / sizeof(Elf_Rel);
    
                Elf_Rel* rel = (Elf_Rel*) ((uintptr_t) dl->header + shdr->sh_offset);
                Elf_Rel* end = &rel[n];
                Elf_Sym* symtab = (Elf_Sym*) ((uintptr_t) dl->header + dl->symtab->sh_offset);

                for(; rel < end; rel++) {
                    Elf_Addr* ptr = (Elf_Addr*) ((uintptr_t) dl->start + dl->sections[shdr->sh_info].sh_addr + rel->r_offset);
                    Elf_Sym* sym = &symtab[ELF_REL_SYM(rel->r_info)];

                    switch(ELF_REL_TYPE(rel->r_info)) {
                        case R_386_32:
                            *ptr += sym->st_value;
                            break;
                        case R_386_PC32:
                            *ptr += sym->st_value - (Elf_Addr) ptr;
                            break;

                        default:
                            return -EME_UNSUPPORTED;
                    }
                }
#else
#   if DEBUG
                fprintf(stderr, "libdl: found unsupported platform for SHT_REL section in %s\n", dl->libname);
#   endif
                __dlerrno = EME_UNSUPPORTED;
                return -1;
#endif
            } break;
            case SHT_RELA:
#if DEBUG
                fprintf(stderr, "libdl: found unsupported SHT_RELA section in %s\n", dl->libname);
#endif
                __dlerrno = EME_UNSUPPORTED;
                return -1;
        }

        shdr++;
    }

    return 0;
}




static inline int dl_elf_link(dl_t* dl) {
    int err;
    int n = dl->symtab->sh_size / sizeof(Elf_Sym);

    Elf_Sym* sym;
    Elf_Sym* symtab = (Elf_Sym*) elf_sec_ptr(dl, dl->symtab);
    Elf_Sym* end = &symtab[n];

    for(sym = &symtab[1]; sym < end; sym++) {
        switch(sym->st_shndx) {
            case SHN_COMMON:
                continue;

            case SHN_ABS:
                continue;

            case SHN_UNDEF:
                sym->st_value = (Elf_Addr) __dl_resolve(NULL, elf_sym_name(dl, sym->st_name));
                break;
            default:
                sym->st_value += (Elf_Addr) elf_get_ptr(dl, dl->sections[sym->st_shndx].sh_addr);

                if(ELF_SYM_TYPE(sym->st_info) != STT_SECTION) {
                    if(__dl_define(dl, elf_sym_name(dl, sym->st_name), (void*) sym->st_value) != 0)
                        return -1;
                }

                break;
        }
    }

    return 0;
}


static inline int dl_elf_load(dl_t* dl, void* image) {
    dl->header = (Elf_Ehdr*) image;

    if(memcmp(dl->header->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1) != 0) {
#if DEBUG
        fprintf(stderr, "libdl: invalid elf library: %s\n", dl->libname);
#endif

        __dlerrno = EME_NOEXEC;
        return -1;
    }


    if(dl->header->e_type != ET_REL) {
#if DEBUG
        fprintf(stderr, "libdl: invalid elf type format: %s\n", dl->libname);
#endif

        __dlerrno = EME_NOEXEC;
        return -1;
    }
    
    dl->sections = (void*) ((uintptr_t) image + dl->header->e_shoff);
    dl->programs = (void*) ((uintptr_t) image + dl->header->e_phoff);
    dl->strings = (void*) ((uintptr_t) image + dl->sections[dl->header->e_shstrndx].sh_offset);
    dl->size = 0;

    int i;
    for(i = 1; i < dl->header->e_shnum; i++) {
        Elf_Shdr* shdr = &dl->sections[i];

        if(shdr->sh_type == SHT_SYMTAB) {
            dl->symtab = &dl->sections[i];
            dl->strtab = &dl->sections[dl->sections[i].sh_link];
            dl->names = (char*) ((uintptr_t) image + dl->strtab->sh_offset);
        }
    }

    dl_elf_layout(dl);

    void* core = (void*) malloc(dl->size);
    if(unlikely(!core)) {
        __dlerrno = EME_NOMEM;
        return -1;
    }
    

    dl->start = core;
    
    Elf_Shdr* shdr;
    for(i = 1; i < dl->header->e_shnum; i++) {
        shdr = &dl->sections[i];

        if(!(shdr->sh_flags & SHF_ALLOC))
            continue;

        memcpy(
            (void*) elf_get_ptr(dl, shdr->sh_addr), 
            (void*) ((uintptr_t) dl->header + shdr->sh_offset),
            shdr->sh_size
        );
    }

    if(unlikely(dl_elf_link(dl) != 0))
        return -1;

    if(unlikely(dl_elf_reloc(dl) != 0))
        return -1;
    

    return 0;
}


void* dlopen(const char* filename, int flags) {

    dl_t* ll;
    for(ll = dl_libs; ll; ll = ll->next) {
        if(strcmp(filename, ll->libname) == 0) {
            ll->flags = flags;
            ll->refcount += 1;
            return (void*) ll;
        }
    }

    if(flags & RTLD_NOLOAD)
        return NULL;



    static char buf[BUFSIZ];
    void* image = NULL;
    dl_t* dl = NULL;
    size_t size;

    #define ERR(x) {        \
        if(image)           \
            free(image);    \
        if(dl)              \
            free(dl);       \
                            \
        __dlerrno = x;      \
        return NULL;        \
    }

    
    int fd = -1, i;
    for(i = 0; dl_path[i]; i++) {
        memset(buf, 0, BUFSIZ);

        sprintf(buf, "%s%s", dl_path[i], filename);
        fd = open(buf, O_RDONLY);
        if(fd >= 0)
            break;
    }

    if(unlikely(fd < 0))
        ERR(EME_NOENT);



    lseek(fd, 0, SEEK_END);
    size = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);

    image = (void*) malloc(size);
    if(unlikely(!image))
        ERR(EME_NOMEM);

    if(unlikely(read(fd, image, size) != size))
        ERR(EME_IO);
    close(fd);


    dl = (dl_t*) malloc(sizeof(dl_t));
    if(unlikely(!dl))
        ERR(EME_NOMEM);

    dl->libname = strdup(filename);
    dl->flags = flags;
    dl->refcount = 1;
    
    int e;
    if((e = dl_elf_load(dl, image)) != 0)
        ERR(e);


    free(image);

    dl->next = dl_libs;
    dl_libs = dl;
    return (void*) dl;
}