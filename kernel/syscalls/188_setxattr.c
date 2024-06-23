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


/***
 * Name:        setxattr
 * Description: set an extended attribute value
 * URL:         http://man7.org/linux/man-pages/man2/setxattr.2.html
 *
 * Input Parameters:
 *  0: 0xbc
 *  1: const char  * path
 *  2: const char  * name
 *  3: const void  * value
 *  4: size_t size
 *  5: int flags
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(188, setxattr, long sys_setxattr(const char *path, const char *name, const void *value, size_t size, int flags) { return -ENOSYS; });
