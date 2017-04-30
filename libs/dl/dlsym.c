#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>

#include "dl.h"


void *dlsym(void *handle, const char *symbol) {
    dl_t* dl = (dl_t*) handle;
    
    symbol_t* sym;
    for(sym = dl->symbols; sym; sym = sym->next)
        if(strcmp(sym->name, symbol) == 0)
            return sym->addr;

    __dlerrno = EME_UNDEFINED_REFERENCE;
    return NULL;
}