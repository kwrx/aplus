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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>


#define __prio_by(property) {                   \
    if(who == 0)                                \
        return current_task->priority;          \
                                                \
    task_t* tmp;                                \
    for(tmp = task_queue; tmp; tmp = tmp->next) \
        if(tmp->property == who)                \
            return tmp->priority;               \
                                                \
    errno = ESRCH;                              \
    return -1;                                  \
}

SYSCALL(96, getpriority,
int sys_getpriority(int which, id_t who) {
    errno = 0;

    switch(which) {
        case PRIO_PROCESS:
            __prio_by(pid);
        case PRIO_PGRP:
            __prio_by(pgid);
        case PRIO_USER:
            __prio_by(uid);
        default:
            break;
    }

    errno = EINVAL;
    return -1;
});
