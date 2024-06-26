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
 * Name:        fchownat
 * Description: change ownership of a file
 * URL:         http://man7.org/linux/man-pages/man2/fchownat.2.html
 *
 * Input Parameters:
 *  0: 0x104
 *  1: int dfd
 *  2: const char  * filename
 *  3: uid_t user
 *  4: gid_t group
 *  5: int flag
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(260, fchownat, long sys_fchownat(int dfd, const char* filename, uid_t user, gid_t group, int flag) { return -ENOSYS; });
