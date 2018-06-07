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

SYSCALL(104, setitimer,
int sys_setitimer(int which, const struct itimerval* newval, struct itimerval* oldval) {
    if(which > 2) {
        errno = EINVAL;
        return -1;
    }
    
    if(oldval)
        sys_getitimer(which, oldval);

    if(!newval)
        return 0;



    ktime_t tm = (timer_gettimestamp() * 1000000) +
                 (timer_getus() % 1000000);
    
    current_task->itimers[which].it_value = tm + (newval->it_value.tv_sec * 1000000) +
                                                 (newval->it_value.tv_usec % 1000000);
    
    current_task->itimers[which].it_interval = (newval->it_interval.tv_sec * 1000000) +
                                               (newval->it_interval.tv_usec % 1000000);
    return 0;
});
