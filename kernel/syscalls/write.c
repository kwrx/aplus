/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(17, write,
int sys_write(int fd, void* buf, size_t size) {
    if(unlikely(fd < 0)) {
        errno = EBADF;
        return -1;
    }
    
    if(unlikely(fd >= TASK_FD_COUNT)) {
#if CONFIG_NETWORK
        return lwip_write(fd - TASK_FD_COUNT, buf, size);
#else
        errno = EBADF;
        return -1;
#endif
    }

    inode_t* inode = current_task->fd[fd].inode;
    
    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(!(
        (current_task->fd[fd].flags & O_WRONLY)    ||
        (current_task->fd[fd].flags & O_RDWR)
    ))) {
        errno = EPERM;
        return -1;
    }


    if(unlikely(current_task->fd[fd].flags & O_NONBLOCK)) {
        struct pollfd p;
        p.fd = fd;
        p.events = POLLOUT;
        p.revents = 0;

        if(sys_poll(&p, 1, 0) < 0) {
            errno = EIO;
            return -1;
        }

        if(!(p.revents & POLLOUT)) {
            errno = EAGAIN;
            return 0;
        }
    }

    current_task->iostat.wchar += (uint64_t) size;
    current_task->iostat.syscw += 1;


    register int e = vfs_write(inode, buf, current_task->fd[fd].position, size);
    if(unlikely(e <= 0))
        return 0;
    
    current_task->fd[fd].position += e;
    current_task->iostat.write_bytes += (uint64_t) e;
    return e;
});
