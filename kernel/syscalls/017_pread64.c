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
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus/hal.h>



/***
 * Name:        pread64
 * Description: read from or write to a file descriptor at a given
       offset
 * URL:         http://man7.org/linux/man-pages/man2/pread64.2.html
 *
 * Input Parameters:
 *  0: 0x11
 *  1: unsigned int fd
 *  2: char  * buf
 *  3: size_t count
 *  4: loff_t pos
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(17, pread64,
long sys_pread64 (unsigned int fd, char  * buf, size_t count, off_t pos) {

    DEBUG_ASSERT(current_task);

    current_task->iostat.rchar += (uint64_t) count;
    current_task->iostat.syscr += 1;


    if(unlikely(count == 0))
        return 0;

    if(unlikely(!uio_check(buf, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(fd >= CONFIG_OPEN_MAX))
        return -EBADF;



    ssize_t e = 0;

    shared_ptr_access(current_task->fd, fds, {

        if(unlikely(!fds->descriptors[fd].ref))
            return -EBADF;


        if(unlikely(!(
            !(fds->descriptors[fd].flags & O_WRONLY) ||
             (fds->descriptors[fd].flags & O_RDONLY)
        )))
            return -EPERM;



        uio_lock(buf, count);

        __lock(&fds->descriptors[fd].ref->lock, {

            if((e = vfs_read(fds->descriptors[fd].ref->inode, buf, pos, count)) <= 0)
                break;

            current_task->iostat.read_bytes += (uint64_t) e;
            
        });

        uio_unlock(buf, count);

    });


    if(e < 0)
        return -errno;

    return e;
    
});
