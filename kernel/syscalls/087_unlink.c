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
#include <aplus/errno.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


extern long sys_unlinkat(int dfd, const char* pathname, int flag);


/***
 * Name:        unlink
 * Description: delete a name and possibly the file it refers to
 * URL:         http://man7.org/linux/man-pages/man2/unlink.2.html
 *
 * Input Parameters:
 *  0: 0x57
 *  1: const char  * pathname
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(87, unlink, long sys_unlink(const char* pathname) { return sys_unlinkat(AT_FDCWD, pathname, 0); });
