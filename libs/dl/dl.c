#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

#include <aplus/base.h>
#include <aplus/elf.h>
#include <aplus/utils/list.h>

#include "dl.h"

list(dl_t*, __dl_loaded);

dl_t* __dl_load(int fd, int flags) {
    

    return NULL;
}