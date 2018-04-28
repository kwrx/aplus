#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_pid_init(procfs_entry_t* e) {
    if(e->task)
        sprintf(e->name, "%d", e->task->pid);
    else
        strcpy(e->name, "self");

    e->mode = S_IFDIR;


    list_push(e->childs, procfs_mkentry(e, e->task, cmdline));
    list_push(e->childs, procfs_mkentry(e, e->task, environ));
    list_push(e->childs, procfs_mkentry(e, e->task, cwd));
    list_push(e->childs, procfs_mkentry(e, e->task, exe));
    list_push(e->childs, procfs_mkentry(e, e->task, root));
    list_push(e->childs, procfs_mkentry(e, e->task, fd));
    list_push(e->childs, procfs_mkentry(e, e->task, io));
    list_push(e->childs, procfs_mkentry(e, e->task, stat));
    list_push(e->childs, procfs_mkentry(e, e->task, statm));
    list_push(e->childs, procfs_mkentry(e, e->task, status));


    return 0;
}


int procfs_pid_update(procfs_entry_t* e) {
    return 0;
}