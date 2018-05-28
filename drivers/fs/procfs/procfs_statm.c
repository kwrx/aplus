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