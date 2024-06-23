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
 * Name:        ftruncate
 * Description: truncate a file to a specified length
 * URL:         http://man7.org/linux/man-pages/man2/ftruncate.2.html
 *
 * Input Parameters:
 *  0: 0x4d
 *  1: unsigned int fd
 *  2: unsigned long length
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    77, ftruncate, long sys_ftruncate(unsigned int fd, unsigned long length) {
        if (unlikely(fd >= CONFIG_OPEN_MAX))
            return -EBADF;


        int e = 0;

        shared_ptr_access(current_task->fd, fds, {
            if (unlikely(!fds->descriptors[fd].ref))
                return -EBADF;

            __lock(&fds->descriptors[fd].ref->lock, { e = vfs_truncate(fds->descriptors[fd].ref->inode, length); });
        });


        if (e < 0)
            return -errno;

        return e;
    });
