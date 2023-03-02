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

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <aplus/hal.h>




/***
 * Name:        newlstat
 * Description: 
 * URL:         http://man7.org/linux/man-pages/man2/newlstat.2.html
 *
 * Input Parameters:
 *  0: 0x06
 *  1: const char __user * filename
 *  2: struct stat __user * statbuf
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(6, newlstat,
long sys_newlstat (const char __user * filename, struct stat __user * statbuf) {

    if(!filename)
        return -EINVAL;

    if(!statbuf)
        return -EINVAL;

    if(!uio_check(filename, R_OK))
        return -EFAULT;

    if(!uio_check(statbuf, R_OK | W_OK))
        return -EFAULT;

    
    int fd;

#if defined(O_NOFOLLOW)
    if((fd = sys_open(filename, O_RDONLY | O_NOFOLLOW, 0)) < 0)
        return fd;
#else
    if((fd = sys_open(filename, O_RDONLY, 0)) < 0)
        return fd;    
#endif


    int e;
    struct stat __statbuf = { 0 };

    shared_ptr_access(current_task->fd, fds, {

        DEBUG_ASSERT(fds->descriptors[fd].ref);

        __lock(&fds->descriptors[fd].ref->lock, {

            e = vfs_getattr(fds->descriptors[fd].ref->inode, &__statbuf);

        });

    });


    if((fd = sys_close(fd)) < 0)
        return fd;


    if(e < 0)
        return -errno;


    uio_memcpy_s2u(statbuf, &__statbuf, sizeof(struct stat));

    return 0;

});
