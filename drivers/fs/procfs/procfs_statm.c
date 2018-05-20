#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_statm_init(procfs_entry_t* e) {
    return 0;
}


int procfs_statm_update(procfs_entry_t* e) {
    volatile task_t* tk = e->task
                            ? e->task : current_task;

    sprintf(e->data,
        "%lu %lu %u %u %u %u %u\n",
        (unsigned long) tk->vmsize / PAGE_SIZE,
        (unsigned long) (tk->image->end - tk->image->start) / PAGE_SIZE,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) CONFIG_STACK_SIZE / PAGE_SIZE,
        (unsigned long) 0
    );

    e->size = strlen(e->data);
    return 0;
}