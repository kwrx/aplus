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

SYSCALL(63, dup2,
int sys_dup2(int oldfd, int newfd) {
    if(oldfd < 0 || oldfd >= TASK_FD_COUNT) {
        errno = EBADF;
        return -1;
    }

    if(newfd < 0 || newfd >= TASK_FD_COUNT) {
        errno = EBADF;
        return -1;
    }

    if(!current_task->fd[oldfd].inode) {
        errno = EBADF;
        return -1;
    }

    if(oldfd == newfd)
        return newfd;

    if(current_task->fd[newfd].inode)
        sys_close(newfd);

    memcpy((void*) &current_task->fd[newfd], (void*) &current_task->fd[oldfd], sizeof(fd_t));
    return newfd;
});