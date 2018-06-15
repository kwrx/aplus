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

SYSCALL(174, rt_sigaction,
int sys_rt_sigaction(int sig, const struct sigaction* act, const struct sigaction* oact, size_t setsize) {
    KASSERT(current_task);

    if(sig < 0 || sig > TASK_NSIG) {
        return errno = EINVAL;
        return -1;
    }

    if(oact)
        memcpy(oact, &current_task->signal.s_handlers[sig], sizeof(struct sigaction));

    if(act)
        memcpy(&current_task->signal.s_handlers[sig], act, sizeof(struct sigaction));
    
    return 0;
});
