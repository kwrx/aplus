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
#include <aplus/hal.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


extern long sys_openat(int dfd, const char* filename, int flags, mode_t mode);
extern long sys_fchmod(unsigned int fd, mode_t mode);

/***
 * Name:        fchmodat
 * Description: change permissions of a file
 * URL:         http://man7.org/linux/man-pages/man2/fchmodat.2.html
 *
 * Input Parameters:
 *  0: 0x10c
 *  1: int dfd
 *  2: const char  * filename
 *  3: umode_t mode
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    268, fchmodat, long sys_fchmodat(int dfd, const char* filename, mode_t mode) {
        if (unlikely(!filename))
            return -EINVAL;

        if (unlikely(!uio_check(filename, R_OK)))
            return -EFAULT;


        int fd = -1;
        int e  = -1;

        if ((fd = sys_openat(dfd, filename, O_RDONLY, 0)) < 0)
            return fd;

        e = sys_fchmod(fd, mode);

        if ((fd = sys_close(fd)) < 0)
            return fd;


        if (e < 0)
            return -errno;

        return e;
    });
