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
 * Name:        getpeername
 * Description: get name of connected peer socket
 * URL:         http://man7.org/linux/man-pages/man2/getpeername.2.html
 *
 * Input Parameters:
 *  0: 0x34
 *  1: int
 *  2: struct sockaddr  *
 *  3: int  *
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

struct sockaddr;

SYSCALL(52, getpeername, long sys_getpeername(int fd, struct sockaddr *sockaddr, int *socklen) { return -ENOSYS; });
