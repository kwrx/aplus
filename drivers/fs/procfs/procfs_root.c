#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_root_init(procfs_entry_t* e) {
    e->mode = S_IFLNK;
    return 0;
}


int procfs_root_update(procfs_entry_t* e) {
    e->link = (void*) (
        e->task 
            ? e->task->root
            : current_task->root
    );

    return 0;
}