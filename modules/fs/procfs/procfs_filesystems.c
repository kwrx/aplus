#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_filesystems_init(procfs_entry_t* e) {
    fsys_t* tmp;
    for(tmp = fsys_queue; tmp; tmp = tmp->next) {
        strcat(e->data, tmp->name);
        strcat(e->data, "\n");
    }

    e->size = strlen(e->data);
    return 0;
}


int procfs_filesystems_update(procfs_entry_t* e) {
    return 0;
}