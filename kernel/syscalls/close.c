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
#include <libc.h>

SYSCALL(1, close,
int sys_close(int fd) {
    if(unlikely(fd < 0)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(fd >= TASK_FD_COUNT)) {
#if CONFIG_NETWORK
        return lwip_close(fd - TASK_FD_COUNT);
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

    current_task->fd[fd].inode = NULL;
    current_task->fd[fd].flags = 0;

    return vfs_close(inode);
});
