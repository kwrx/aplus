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