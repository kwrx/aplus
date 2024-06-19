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

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <aplus/hal.h>


extern long sys_newfstatat (int dfd, const char __user * filename, struct stat __user * statbuf, int flag);


/***
 * Name:        newstat
 * Description: 
 * URL:         http://man7.org/linux/man-pages/man2/newstat.2.html
 *
 * Input Parameters:
 *  0: 0x04
 *  1: const char __user * filename
 *  2: struct stat __user * statbuf
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(4, newstat,
long sys_newstat (const char __user * filename, struct stat __user * statbuf) {
    return sys_newfstatat(AT_FDCWD, filename, statbuf, 0);
});
