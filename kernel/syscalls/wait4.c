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


#define __wait_for(cond)                                            \
    {                                                               \
        volatile task_t* v;                                         \
        for(v = task_queue; v; v = v->next) {                       \
            if(v->parent != current_task)                           \
                continue;                                           \
                                                                    \
            if((cond))                                              \
                list_push(current_task->waiters, v);                \
        }                                                           \
    }


SYSCALL(114, wait4,
int sys_wait4(pid_t pid, int* status, int options, struct rusage* usage) {
    if(pid < -1)
        __wait_for(v->pgid == -pid)
    else if(pid == -1)
        __wait_for(1)
    else if(pid == 0)
        __wait_for(v->pgid == current_task->pgid)
    else if(pid > 0)
        __wait_for(v->pid == pid)


    if(list_length(current_task->waiters) == 0) {
        errno = ECHILD;
        return -1;
    }

    if(options & WNOHANG) {
        list_clear(current_task->waiters);
        return 0;
    }

    syscall_ack();
    sys_pause();


    pid_t p = -1;
    list_each(current_task->waiters, w) {
        if(w->status != TASK_STATUS_ZOMBIE && w->status != TASK_STATUS_STOP)
            continue;


        if(w->status == TASK_STATUS_ZOMBIE) {
            if(w == task_queue)
                task_queue = w->next;
            else {
                volatile task_t* tmp;
                for(tmp = task_queue; tmp; tmp = tmp->next) {
                    if(tmp->next == w)
                        tmp->next = w->next;
                }
            }
        }

             
        if(status)
            *status = (int) w->exit.value;

        if(usage)
            memcpy(usage, &w->rusage, sizeof(struct rusage));
        
        p = w->pid;
        break;
    }

    list_clear(current_task->waiters);


    if(unlikely(p == -1))
        errno = EINTR;

    return p;
});
