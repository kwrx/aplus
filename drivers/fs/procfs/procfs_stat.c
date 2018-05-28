/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


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
            (tk->status == TASK_STATUS_STOP    ? 'T'   :
            (tk->status == TASK_STATUS_RUNNING ? 'R'   :
            (tk->status == TASK_STATUS_ZOMBIE  ? 'Z'   : 'X')))))
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