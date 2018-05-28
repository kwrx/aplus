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
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>


SYSCALL(90, sysconf,
long sys_sysconf(int n) {
    errno = 0;

    switch(n) {
        #define __case(x, y) case x: return y

        __case(_SC_ARG_MAX, 4096);
        __case(_SC_CHILD_MAX, -1);
        __case(_SC_NGROUPS_MAX, 1);
        __case(_SC_CLK_TCK, CLOCKS_PER_SEC);
        __case(_SC_OPEN_MAX, 20);
        __case(_SC_PAGESIZE, PAGE_SIZE);
        __case(_SC_JOB_CONTROL, -1);
        __case(_SC_SAVED_IDS, 0);
        __case(_SC_VERSION, 20170101L);
        
        default:
            errno = EINVAL;
            return -1;
    }
});
