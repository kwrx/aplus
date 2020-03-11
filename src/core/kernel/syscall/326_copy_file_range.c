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
 * Name:        copy_file_range
 * Description: Copy a range of data from one file to another
 * URL:         http://man7.org/linux/man-pages/man2/copy_file_range.2.html
 *
 * Input Parameters:
 *  0: 0x146
 *  1: undefined
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(326, copy_file_range,
long sys_copy_file_range (int fd_in, off_t __user * off_in, int fd_out, off_t __user * off_out, size_t len, unsigned int flags) {
    return -ENOSYS;
});
