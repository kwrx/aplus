#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <libgen.h>

#include "dl.h"

static int find_library(const char* filename) {
    int tryopen(char* at, const char* filename) {
        char buf[BUFSIZ];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "%s/%s", at, filename);

        return open(buf, O_RDONLY);
    }


    if(filename[0] == '/')
        return tryopen("", &filename[1]);

    int fd = -1;
    if((fd = tryopen(".", filename)) >= 0)
        return tryopen("", &filename[1]);

    if((fd = tryopen("/lib", filename)) >= 0)
        return fd;

    if((fd = tryopen("/usr/lib", filename)) >= 0)
        return fd;

    char* s = getenv("LD_LIBRARY_PATH");
    if(!s)
        return -1;

    s = strdup(s);
    for(char* p = strtok(s, ":"); p; p = strtok(NULL, ":"))
        if((fd = tryopen(p, filename)) >= 0)
            break;

    free(s);
    return fd;
}

void* dlopen(const char* filename, int flag) {
    dl_t* dl = NULL;
    list_each(__dl_loaded, d) {
        if(strcmp(d->filename, filename) != 0)
            continue;

        dl = d;
        dl->flags = flag;
        break;
    }

    if(dl || flag & RTLD_NOLOAD)
        return dl;


    int fd;
    if((fd = find_library(basename((char*) filename))) < 0) {
        __dlerrno = DL_ERR_CANNOT_LOAD_LIBRARY;
        return NULL;
    }

    dl = __dl_load(fd, flag);
    if(!dl) {
        __dlerrno = DL_ERR_CANNOT_LOAD_LIBRARY;
        return NULL;
    }

    strcpy(dl->filename, basename((char*) filename));

    list_push(__dl_loaded, dl);
    return (void*) dl;
}