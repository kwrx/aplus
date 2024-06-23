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
 * Name:        recvmmsg
 * Description: receive multiple messages on a socket
 * URL:         http://man7.org/linux/man-pages/man2/recvmmsg.2.html
 *
 * Input Parameters:
 *  0: 0x12b
 *  1: int fd
 *  2: struct mmsghdr  * msg
 *  3: unsigned int vlen
 *  4: unsigned flags
 *  5: struct timespec  * timeout
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

struct mmsghdr;

SYSCALL(299, recvmmsg, long sys_recvmmsg(int fd, struct mmsghdr* msg, unsigned int vlen, unsigned flags, struct timespec* timeout) { return -ENOSYS; });
