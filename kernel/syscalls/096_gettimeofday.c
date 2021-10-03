/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus/hal.h>





/***
 * Name:        gettimeofday
 * Description: get / set time
 * URL:         http://man7.org/linux/man-pages/man2/gettimeofday.2.html
 *
 * Input Parameters:
 *  0: 0x60
 *  1: struct timeval __user * tv
 *  2: struct timezone __user * tz
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

struct timezone;

SYSCALL(96, gettimeofday,
long sys_gettimeofday (struct timeval __user * tv, struct timezone __user * tz) {
    
    if(unlikely(!tv))
        return -EINVAL;
    
    if(!uio_check(tv, R_OK | W_OK))
        return -EINVAL;


    tv->tv_sec = arch_timer_gettime();
    tv->tv_usec = 0; 

    return 0;
    
});
