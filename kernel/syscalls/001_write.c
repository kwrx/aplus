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

#if defined(CONFIG_HAVE_NETWORK)
#include <aplus/network.h>
#endif


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

    DEBUG_ASSERT(current_task);

    current_task->iostat.wchar += (uint64_t) size;
    current_task->iostat.syscw += 1;


    if(unlikely(size == 0))
        return 0;

    if(unlikely(!buf))
        return -EINVAL;

    if(unlikely(!uio_check(buf, R_OK)))
        return -EFAULT;



    ssize_t e = 0;

#if defined(CONFIG_HAVE_NETWORK)

    if(unlikely(NETWORK_IS_SOCKFD(fd))) {

        uio_lock(buf, size);

        e = lwip_write(NETWORK_SOCKFD(fd), buf, size);

        uio_unlock(buf, size);


        if(unlikely(e < 0))
            return -errno;

        return e;

    } else
    
#endif
    
     {

        if(unlikely(fd >= CONFIG_OPEN_MAX))
            return -EBADF;

        
        shared_ptr_access(current_task->fd, fds, {

            if(unlikely(!fds->descriptors[fd].ref))
                return -EBADF;

            if(unlikely(!(
                (fds->descriptors[fd].flags & O_WRONLY) ||
                (fds->descriptors[fd].flags & O_RDWR)
            )))
                return -EPERM;



            ssize_t e = 0;


            uio_lock(buf, size);

            __lock(&fds->descriptors[fd].ref->lock, {

                if((e = vfs_write(fds->descriptors[fd].ref->inode, buf, fds->descriptors[fd].ref->position, size)) <= 0)
                    break;

                fds->descriptors[fd].ref->position += e;
                current_task->iostat.write_bytes += (uint64_t) e;

            });

            uio_unlock(buf, size);


            if(errno == EINTR) {

                if(fds->descriptors[fd].flags & O_NONBLOCK) {

                    return -EAGAIN;

                } else {

                    shared_ptr_nullable_access(fds->descriptors[fd].ref->inode->ev, ev, {

                        ev->revents &= ~POLLOUT;
                        ev->events  |=  POLLOUT;
                        ev->futex    = 0;

                        futex_wait(current_task, &ev->futex, 0, NULL);

                    });


    #if DEBUG_LEVEL_TRACE
                    kprintf("read: task %d waiting for POLLOUT event on fd %d (node->name: '%s')\n", current_task->tid, fd, fds->descriptors[fd].ref->inode->name);
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

    }

});
