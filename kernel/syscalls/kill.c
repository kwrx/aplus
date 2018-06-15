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
#include <libc.h>


SYSCALL(37, kill,
int sys_kill(pid_t pid, int signal) {
    volatile task_t* tmp;
    int r = 0;

    #define _do(cond, ret) {                                        \
        for(tmp = task_queue; tmp; tmp = tmp->next) {               \
            if(current_task->uid != 0)                              \
                if(current_task->uid != tmp->uid)                   \
                    continue;                                       \
                                                                    \
            if(cond) {                                              \
                r++;                                                \
                if(signal == 0)                                     \
                    ret;                                            \
                                                                    \
                                                                    \
                if(sigismember(&tmp->signal.s_mask, signal)) {      \
                    if(pid <= 0)                                    \
                        continue;                                   \
                                                                    \
                    errno = EPERM;                                  \
                    return -1;                                      \
                }                                                   \
                                                                    \
                                                                    \
                siginfo_t si;                                       \
                si.si_code = SI_USER;                               \
                si.si_pid = current_task->pid;                      \
                si.si_uid = current_task->uid;                      \
                si.si_value.sival_ptr = NULL;                       \
                                                                    \
                sched_sigqueueinfo(tmp, signal, &si);               \
                ret;                                                \
            }                                                       \
        }                                                           \
    }

    if(pid > 0)
        _do(tmp->pid == pid, return 0)
    else if(pid == 0)
        _do(tmp->pgid == current_task->pgid, continue)
    else if(pid < -1)
        _do(tmp->pgid == abs(pid), continue)
    else {
        if(current_task->uid != 0) {
            errno = EPERM;
            return -1;
        }

        _do(tmp != current_task, continue)
    }
    


    if(r)
        return 0;
        
    errno = ESRCH;
    return -1;
});
