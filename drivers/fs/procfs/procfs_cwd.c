#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_cwd_init(procfs_entry_t* e) {
    e->mode = S_IFLNK;
    return 0;
}


int procfs_cwd_update(procfs_entry_t* e) {
    e->link = (void*) (
        e->task 
            ? e->task->cwd
            : current_task->cwd
    );

    return 0;
}