#pragma once

#include <aplus/base.h>
#include <aplus/utils/list.h>

typedef struct {
    char filename[BUFSIZ];
    int flags;
} dl_t;

extern int __dlerrno;
extern list(dl_t*, __dl_loaded);

#define DL_SUCCESS                          0
#define DL_ERR_CANNOT_LOAD_LIBRARY          1
#define DL_ERR_INVALID_LIBRARY_HANDLE       2
#define DL_ERR_BAD_SYMBOL_NAME              3
#define DL_ERR_SYMBOL_NOT_FOUND             4
#define DL_ERR_SYMBOL_NOT_GLOBAL            5