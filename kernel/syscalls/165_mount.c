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

#include <fcntl.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/vfs.h>



/***
 * Name:        mount
 * Description: mount filesystem
 * URL:         http://man7.org/linux/man-pages/man2/mount.2.html
 *
 * Input Parameters:
 *  0: 0xa5
 *  1: char  * dev_name
 *  2: char  * dir_name
 *  3: char  * type
 *  4: unsigned long flags
 *  5: void  * data
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    165, mount, long sys_mount(char const* dev_name, char const* dir_name, char const* type, unsigned long flags, void* data) {
        if (unlikely(!dir_name))
            return -EINVAL;

        if (unlikely(!type))
            return -EINVAL;


        if (unlikely(!uio_check(dir_name, R_OK)))
            return -EFAULT;

        if (unlikely(!uio_check(type, R_OK)))
            return -EFAULT;

        if (unlikely(dev_name && !uio_check(dev_name, R_OK)))
            return -EFAULT;


        char __safe_dirname[CONFIG_PATH_MAX];
        uio_strncpy_u2s(__safe_dirname, dir_name, CONFIG_PATH_MAX);

        char __safe_type[CONFIG_PATH_MAX];
        uio_strncpy_u2s(__safe_type, type, CONFIG_PATH_MAX);



        inode_t* s = NULL;
        inode_t* d = NULL;

        int fd;
        int e;

        if ((fd = sys_open(__safe_dirname, O_RDONLY | O_DIRECTORY, 0)) < 0)
            return fd;


        shared_ptr_access(current_task->fd, fds, {
            DEBUG_ASSERT(fds->descriptors[fd].ref);
            DEBUG_ASSERT(fds->descriptors[fd].ref->inode);

            d = fds->descriptors[fd].ref->inode;
        });


        if ((e = sys_close(fd)) < 0)
            return e;



        if (likely(dev_name) && !(flags & MS_NODEV)) {

            if ((fd = sys_open(dev_name, O_RDONLY, 0)) < 0)
                return fd;

            shared_ptr_access(current_task->fd, fds, {
                DEBUG_ASSERT(fds->descriptors[fd].ref);
                DEBUG_ASSERT(fds->descriptors[fd].ref->inode);

                s = fds->descriptors[fd].ref->inode;
            });

            if ((e = sys_close(fd)) < 0)
                return e;
        }


        DEBUG_ASSERT(d);

        if (vfs_mount(s, d, __safe_type, flags, (const char*)data) < 0)
            return -errno;


        return 0;
    });
