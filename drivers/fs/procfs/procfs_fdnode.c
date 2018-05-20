#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_fdnode_init(procfs_entry_t* e) {
    sprintf(e->name, "%d", (int) e->arg);
    e->mode = S_IFLNK;
    return 0;
}


int procfs_fdnode_update(procfs_entry_t* e) {
    e->link = (void*) (
        e->task 
            ? e->task->fd[(int) e->arg].inode
            : current_task->fd[(int) e->arg].inode
    );

    return 0;
}