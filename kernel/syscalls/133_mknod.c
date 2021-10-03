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
 * Name:        mknod
 * Description: create a special or ordinary file
 * URL:         http://man7.org/linux/man-pages/man2/mknod.2.html
 *
 * Input Parameters:
 *  0: 0x85
 *  1: const char __user * filename
 *  2: umode_t mode
 *  3: unsigned dev
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(133, mknod,
long sys_mknod (const char __user * filename, mode_t mode, unsigned dev) {
    return -ENOSYS;
});
