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
#include <poll.h>
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
 * Name:        write
 * Description: write to a file descriptor
 * URL:         http://man7.org/linux/man-pages/man2/write.2.html
 *
 * Input Parameters:
 *  0: 0x01
 *  1: unsigned int fd
 *  2: const char __user * buf
 *  3: size_t size
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(1, write,
long sys_write (unsigned int fd, const void __user * buf, size_t size) {

    if(unlikely(!uio_check(buf, R_OK)))
        return -EFAULT;

    if(unlikely(fd > CONFIG_OPEN_MAX)) // TODO: add network support
        return -EBADF;

    if(unlikely(!current_task->fd->descriptors[fd].ref))
        return -EBADF;

    if(unlikely(!(
        (current_task->fd->descriptors[fd].flags & O_WRONLY) ||
        (current_task->fd->descriptors[fd].flags & O_RDWR)
    )))
        return -EPERM;



    current_task->iostat.wchar += (uint64_t) size;
    current_task->iostat.syscw += 1;


    if(unlikely(size == 0))
        return 0;


    ssize_t e = 0;

    __lock(&current_task->fd->descriptors[fd].ref->lock, {

        if((e = vfs_write(current_task->fd->descriptors[fd].ref->inode, uio_get_ptr(buf), current_task->fd->descriptors[fd].ref->position, size)) <= 0)
            break;

        current_task->fd->descriptors[fd].ref->position += e;
        current_task->iostat.write_bytes += (uint64_t) e;

    });


    if(errno == EINTR) {

        if(current_task->fd->descriptors[fd].flags & O_NONBLOCK) {

            return -EAGAIN;

        } else {

            current_task->fd->descriptors[fd].ref->inode->ev.revents &= ~POLLOUT;
            current_task->fd->descriptors[fd].ref->inode->ev.events  |=  POLLOUT;
            current_task->fd->descriptors[fd].ref->inode->ev.futex    = 0;

            futex_wait(current_task, &current_task->fd->descriptors[fd].ref->inode->ev.futex, 0, NULL);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
            kprintf("write: task %d waiting for POLLOUT event\n", current_task->tid);
#endif

            thread_suspend(current_task);
            thread_restart_sched(current_task);
            thread_restart_syscall(current_task);

            return -EINTR;

        }

    }


    if(e < 0)
        return -errno;

    return e;

});
