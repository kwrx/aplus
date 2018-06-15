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
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(175, rt_sigprocmask,
int sys_rt_sigprocmask(int how, const sigset_t* set, sigset_t* old, size_t setsize) {
    KASSERT(current_task);

    if(likely(old))
        memcpy(old, &current_task->signal.s_mask, sizeof(sigset_t));

    if(unlikely(!set))
        return 0;
        
    
    switch(how) {
        case SIG_BLOCK:
            for(int i = 0; i < _NSIG; i++)
                if(set->__bits[i])
                    current_task->signal.s_mask.__bits[i] = 1;
            break;
        case SIG_UNBLOCK:
            for(int i = 0; i < _NSIG; i++)
                if(set->__bits[i])
                    current_task->signal.s_mask.__bits[i] = 0;
            break;
        case SIG_SETMASK:
            memcpy(&current_task->signal.s_mask, set, sizeof(sigset_t));
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
});