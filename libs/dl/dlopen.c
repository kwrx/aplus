/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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