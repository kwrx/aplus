#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_stat_init(procfs_entry_t* e) {
    return 0;
}


int procfs_stat_update(procfs_entry_t* e) {
    volatile task_t* tk = e->task
                            ? e->task : current_task;

   
    sprintf(e->data,
        "%d (%s) %c %d %d "
        "%d %d %d %u %lu "
        "%lu %lu %lu %lu %lu "
        "%ld %ld %ld %ld %ld "
        "%ld %lu %lu %ld %lu "
        "%lu %lu %lu %lu %lu "
        "%lu %lu %lu %lu %lu "
        "%lu %lu %d %d %u "
        "%u %lu %lu %ld %lu "
        "%lu %lu %lu %lu %lu "
        "%lu %d\n",

        tk->pid,
        tk->name,
        (
            (tk->status == TASK_STATUS_READY   ? 'S'   :
            (tk->status == TASK_STATUS_SLEEP   ? 'S'   :
            (tk->status == TASK_STATUS_RUNNING ? 'R'   :
            (tk->status == TASK_STATUS_KILLED  ? 'Z'   : 'X'))))
        ),
        tk->parent ? tk->parent->pid : 0,
        tk->pgid,
        tk->sid,
        0,
        0,
        0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) tk->clock.tms_utime,
        (unsigned long) tk->clock.tms_stime,
        (unsigned long) tk->clock.tms_cutime,
        (unsigned long) tk->clock.tms_cstime,
        (long) tk->priority,
        (long) tk->priority,
        (long) 1,
        (long) 0,
        (unsigned long ) tk->starttime,
        (unsigned long) tk->vmsize,
        (long) tk->vmsize / PAGE_SIZE,
        (unsigned long) -1,
        (unsigned long) tk->image->start,
        (unsigned long) tk->image->end,
        (unsigned long) CONFIG_STACK_BASE,
        (unsigned long) tk->context,
        (unsigned long) tk->context,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        (unsigned long) 0,
        SIGKILL,
        1,
        0,
        0,
        (unsigned long) 0,
        (unsigned long) 0,
        (long) 0,
        (unsigned long) tk->image->start,
        (unsigned long) tk->image->end,
        (unsigned long) tk->image->end,
        (unsigned long) tk->argv,
        (unsigned long) tk->argv,
        (unsigned long) tk->environ,
        (unsigned long) tk->environ,
        (unsigned long) tk->exit.value
    );

    e->size = strlen(e->data);
    return 0;
}