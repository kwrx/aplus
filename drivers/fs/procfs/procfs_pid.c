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
    list_push(e->childs, procfs_mkentry(e, e->task, mountinfo));
    list_push(e->childs, procfs_mkentry(e, e->task, mounts));
    list_push(e->childs, procfs_mkentry(e, e->task, mountstats));



    return 0;
}


int procfs_pid_update(procfs_entry_t* e) {
    return 0;
}