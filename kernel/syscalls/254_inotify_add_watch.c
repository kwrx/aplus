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


/***
 * Name:        inotify_add_watch
 * Description: add a watch to an initialized inotify instance
 * URL:         http://man7.org/linux/man-pages/man2/inotify_add_watch.2.html
 *
 * Input Parameters:
 *  0: 0xfe
 *  1: int fd
 *  2: const char __user * path
 *  3: u32 mask
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(254, inotify_add_watch,
long sys_inotify_add_watch (int fd, const char __user * path, uint32_t mask) {
    return -ENOSYS;
});
