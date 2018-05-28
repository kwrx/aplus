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
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(47, setpgid,
int sys_setpgid(pid_t pid, gid_t pgid) {
    if(pid == 0)
        current_task->pgid = pgid == 0 
                                ? current_task->pid 
                                : pgid
                                ;
    else {
        volatile task_t* tmp;
        for(tmp = task_queue; tmp; tmp = tmp->next) {
            if(tmp->pid == pid) {
                if(tmp->sid != current_task->sid) {
                    errno = EPERM;
                    return -1;
                }
                
                tmp->pgid = pgid == 0 
                                ? tmp->pid 
                                : pgid
                                ;
                return 0;   
            }
        }
    
        errno = ESRCH;
        return -1;
    }

    return 0;
});

SYSCALL(48, getpgid,
gid_t sys_getpgid(pid_t pid) {
    if(pid == 0)
        return current_task->pgid;
    else {
        volatile task_t* tmp;
        for(tmp = task_queue; tmp; tmp = tmp->next) {
            if(tmp->pid == pid)
                return tmp->pgid;
        }
    }

    errno = ESRCH;
    return -1;
});
