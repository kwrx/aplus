#define _WITH_MNTFLAGS 1
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


static char* __mkpath(volatile task_t* tk, inode_t* d, char* buf, size_t len) {
    if(!d) {
        strcpy(buf, "none");
        return buf;
    }

    memset(buf, 0, len);


    int i = 0;    
    while(d && d != tk->root) {
        int j;
        for(j = strlen(d->name) - 1; j >= 0; j--)
            buf[i++] = d->name[j];

        buf[i++] = '/';
        d = d->parent;
    }

    

    if(strlen(buf) > 0) {
        int j, k;
        for(j = strlen(buf) - 1, i = 0, k = 0; k < strlen(buf) / 2; k++, j--, i++) {
            buf[i] ^= buf[j];
            buf[j] ^= buf[i];
            buf[i] ^= buf[j];
        }
    } else 
        buf[0] = '/';

    return buf;
}

static char* __mkopt(int flags, char* buf, size_t len) {
    memset(buf, 0, len);
    
    int i;
    for(i = 0; mnt_flags[i].option; i++) {
        if(flags & mnt_flags[i].value) {
            strcat(buf, mnt_flags[i].option);
            strcat(buf, ",");
        }
    }

    if(strlen(buf) > 0)
        buf[strlen(buf) - 1] = '\0';
    else
        strcpy(buf, "defaults");

    return buf;
}


int procfs_mounts_init(procfs_entry_t* e) {
    return 0;
}


int procfs_mounts_update(procfs_entry_t* e) {
    char d[BUFSIZ];
    char r[BUFSIZ];
    char o[256];

    volatile task_t* tk = e->task;
    if(unlikely(!tk))
        tk = current_task;

    char* p = e->data;
    list_each(mnt_queue, mt) {
        __mkpath(tk, mt->root, d, sizeof(d));
        __mkpath(tk, mt->info->dev, r, sizeof(r));
        __mkopt(mt->info->flags, o, sizeof(o));

        sprintf (p, 
            "%s %s %s %s 0 0\n",
            r, d, mt->info->fstype, o
        );

        p += strlen(p);
    }

    e->size = strlen(e->data);
    return 0;
}