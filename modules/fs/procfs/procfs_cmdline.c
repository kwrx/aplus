#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_cmdline_init(procfs_entry_t* e) {
    volatile task_t* tk = e->task;
    if(unlikely(!tk))
        tk = current_task;

    if(!tk->argv)
        return 0;


    int i, j;
    for(i = j = 0; tk->argv[i]; j += strlen(tk->argv[i++]) + 1)
        strcpy(&e->data[j], tk->argv[i]);

    e->size = j;
    return 0;
}


int procfs_cmdline_update(procfs_entry_t* e) {
    if(!e->task)
        e->init(e);

    return 0;
}