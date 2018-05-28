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