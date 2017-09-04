#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>

#include "dl.h"



dl_t* dl_libs = NULL;
int __dlerrno = EME_OK;

char* dl_path[] = {
    "",
    "./",
    "/lib/",
    "/usr/lib/",
    "/usr/local/lib/",
    "/usr/share/lib/",
    NULL
};





int __dl_define(dl_t* dl, char* name, void* value) {
    symbol_t* sym;
    for(sym = dl->symbols; sym; sym = sym->next) {
        if(strcmp(sym->name, name) != 0)
            continue;

#if DEBUG
        fprintf(stderr, "libdl: symbol %s already defined in %s at 0x%x\n", dl->libname, name, sym->addr);
#endif
        
        __dlerrno = EME_MULTIPLE_DEFINITIONS;
        return -1;
    }


    sym = (symbol_t*) malloc(sizeof(symbol_t) + strlen(name) + 1);
    if(unlikely(!sym)) {
        __dlerrno = EME_NOMEM;
        return -1;
    }

    memset(sym, 0, sizeof(symbol_t) + strlen(name) + 1);
    strcpy(sym->name, name);
    sym->addr = value;

    sym->next = dl->symbols;
    dl->symbols = sym;
    return 0;
}


void* __dl_resolve(dl_t* dl, char* name) {
    symbol_t* sym;

    if(likely(dl)) {
        for(sym = dl->symbols; sym; sym = sym->next) {
            if(strcmp(sym->name, name) != 0)
                continue;
            return sym->addr;
        }
    } else {
        dl_t* ll;
        for(ll = dl_libs; ll; ll = ll->next) {
            if(!(ll->flags & RTLD_GLOBAL))
                continue;

            for(sym = ll->symbols; sym; sym = sym->next) {
                if(strcmp(sym->name, name) != 0)
                    continue;
                return sym->addr;
            }
        }
    }

#if DEBUG
    fprintf(stderr, "libdl: symbol %s undefined\n", name);
#endif

    __dlerrno = EME_UNDEFINED_REFERENCE;
    return NULL;
}
