#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_fd_init(procfs_entry_t* e) {
    e->mode = S_IFDIR;
    return 0;
}


int procfs_fd_update(procfs_entry_t* e) {
    volatile task_t* tk = e->task;
    if(unlikely(!tk))
        tk = current_task;


    list_each(e->childs, v)
        kfree(v);

    list_clear(e->childs);

    int i;
    for(i = 0; i < TASK_FD_COUNT; i++)
        if(tk->fd[i].inode)
            list_push(e->childs, procfs_mkentry_with_arg(e, e->task, fdnode, i));


    return 0;
}