#include <stdio.h>
#include <dlfcn.h>

#include "dl.h"

static char* err[] = {
    [DL_SUCCESS] = "Success",
    [DL_ERR_CANNOT_LOAD_LIBRARY] = "Cannot load library",
    [DL_ERR_INVALID_LIBRARY_HANDLE] = "Invalid library handle",
    [DL_ERR_BAD_SYMBOL_NAME] = "Invalid symbol name",
    [DL_ERR_SYMBOL_NOT_FOUND] = "Symbol not found",
    [DL_ERR_SYMBOL_NOT_GLOBAL] = "Symbol is not global",
};

int __dlerrno = DL_SUCCESS; 

char* dlerror(void) {
    if(__dlerrno > (sizeof(err) / sizeof(err[0])))
        return NULL;

    return err[__dlerrno];
}