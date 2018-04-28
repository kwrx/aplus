#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


procfs_entry_t* __procfs_mkentry(procfs_entry_t* parent, volatile task_t* task, char* name,
                                    int (*init) (procfs_entry_t*), int (*update) (procfs_entry_t*), void* arg) {

    procfs_entry_t* e = (procfs_entry_t*) kmalloc(sizeof(procfs_entry_t), GFP_KERNEL);
    if(!e) {
        kprintf(ERROR "procfs: no memory left when creating entry: %s\n", name);
        return NULL;
    }

    memset(e, 0, sizeof(e));
    e->parent = parent;
    e->task = task;
    e->arg = arg;
    e->size = 0;
    e->mode = S_IFREG;
    e->init = init;
    e->update = update;
    strcpy(e->name, name);

    if(e->init)
        e->init(e);

    return e;
}


int procfs_init(procfs_entry_t* sb) {
    //list_push(sb->childs, procfs_mkentry(sb, NULL, bus));
    //list_push(sb->childs, procfs_mkentry(sb, NULL, cgroups));
    list_push(sb->childs, procfs_mkentry(sb, kernel_task, cmdline));
    //list_push(sb->childs, procfs_mkentry(sb, NULL, cpuinfo));
    //list_push(sb->childs, procfs_mkentry(sb, NULL, devices));
    //list_push(sb->childs, procfs_mkentry(sb, NULL, filesystems));
    list_push(sb->childs, procfs_mkentry(sb, NULL, meminfo));
    //list_push(sb->childs, procfs_mkentry(sb, NULL, modules));
    //list_push(sb->childs, procfs_mkentry(sb, NULL, net));
    list_push(sb->childs, procfs_mkentry(sb, NULL, uptime));
    list_push(sb->childs, procfs_mkentry(sb, NULL, version));
    list_push(sb->childs, procfs_mkentry(sb, NULL, pid));

    return 0;
}


int procfs_update(procfs_entry_t* sb) {

    volatile task_t* tmp;
    for(tmp = task_queue; tmp; tmp = tmp->next) {
        int f = 0;
        list_each(sb->childs, i) {
            if(!isdigit(i->name[0]))
                continue;

            if(i->task != tmp)
                continue;

            f++;
            break;
        }

        if(f)
            continue;

        list_push(sb->childs, procfs_mkentry(sb, tmp, pid));
    }

    return 0;
}