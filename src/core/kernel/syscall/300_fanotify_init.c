/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


/***
 * Name:        fanotify_init
 * Description: create and initialize fanotify group
 * URL:         http://man7.org/linux/man-pages/man2/fanotify_init.2.html
 *
 * Input Parameters:
 *  0: 0x12c
 *  1: unsigned int flags
 *  2: unsigned int event_f_flags
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(300, fanotify_init,
long sys_fanotify_init (unsigned int flags, unsigned int event_f_flags) {
    return -ENOSYS;
});
