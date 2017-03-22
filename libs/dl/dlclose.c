#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dl.h"


int dlclose(void* handle) {
    dl_t* dl = (dl_t*) handle;
    if(--dl->refcount)
        return 0;

    if(dl == dl_libs)
        dl_libs = dl_libs->next;
    else {
        dl_t* ll;
        for(ll = dl_libs; ll->next; ll = ll->next) {
            if(ll->next != dl)
                continue;
            
            ll->next = dl->next;
            break;
        }
    }

    free(dl->start);
    free(dl);
    return 0;
}