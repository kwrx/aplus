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


int procfs_io_init(procfs_entry_t* e) {
    return 0;
}


int procfs_io_update(procfs_entry_t* e) {
    volatile task_t* tk = e->task
                            ? e->task : current_task;

    sprintf(e->data,
        "rchar: %lu\n"
        "wchar: %lu\n"
        "syscr: %lu\n"
        "syscw: %lu\n"
        "read_bytes: %lu\n"
        "write_bytes: %lu\n"
        "cancelled_write_bytes: %lu\n",
        (unsigned long) tk->iostat.rchar,
        (unsigned long) tk->iostat.wchar,
        (unsigned long) tk->iostat.syscr,
        (unsigned long) tk->iostat.syscw,
        (unsigned long) tk->iostat.read_bytes,
        (unsigned long) tk->iostat.write_bytes,
        (unsigned long) tk->iostat.cancelled_write_bytes
    );

    e->size = strlen(e->data);
    return 0;
}