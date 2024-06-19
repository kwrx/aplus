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
 * Name:        fanotify_mark
 * Description: add, remove, or modify an fanotify mark on a filesys‐
       tem object
 * URL:         http://man7.org/linux/man-pages/man2/fanotify_mark.2.html
 *
 * Input Parameters:
 *  0: 0x12d
 *  1: int fanotify_fd
 *  2: unsigned int flags
 *  3: u64 mask
 *  4: int fd
 *  5: const char __user * pathname
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(301, fanotify_mark,
long sys_fanotify_mark (int fanotify_fd, unsigned int flags, uint64_t mask, int fd, const char __user * pathname) {
    return -ENOSYS;
});
