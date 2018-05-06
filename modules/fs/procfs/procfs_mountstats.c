#define _WITH_MNTFLAGS 1
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


static char* __mkpath(volatile task_t* tk, inode_t* d, char* buf, size_t len) {
    if(!d) {
        strcpy(buf, "nodevice");
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



int procfs_mountstats_init(procfs_entry_t* e) {
    return 0;
}


int procfs_mountstats_update(procfs_entry_t* e) {
    char d[BUFSIZ];
    char r[BUFSIZ];

    volatile task_t* tk = e->task;
    if(unlikely(!tk))
        tk = current_task;

    char* p = e->data;
    list_each(mnt_queue, mt) {
        __mkpath(tk, mt->root, d, sizeof(d));
        __mkpath(tk, mt->info->dev, r, sizeof(r));

        sprintf (p, 
            "device %s mounted on %s with fstype %s\n",
            r, d, mt->info->fstype
        );

        p += strlen(p);
    }

    e->size = strlen(e->data);
    return 0;
}
