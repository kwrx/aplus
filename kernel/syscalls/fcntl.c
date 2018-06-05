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

SYSCALL(55, fcntl,
int sys_fcntl(int fd, int cmd, long arg) {
    if(unlikely(fd < 0)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(fd >= TASK_FD_COUNT)) {
#if CONFIG_NETWORK
        return lwip_fcntl(fd - TASK_FD_COUNT, cmd, arg);
#else
        errno = EBADF;
        return -1;
#endif
    }

    switch(cmd) {
        case F_DUPFD:
            if(fd == (int) arg)
                return (int) arg;
                
            sys_close(arg);
            memcpy((void*) &current_task->fd[arg], (void*) &current_task->fd[fd], sizeof(fd_t));
    
            return (int) arg;
        case F_GETFD:
            return 0;
        case F_SETFD:
            return 0;
        case F_GETFL:
            return current_task->fd[fd].flags;
        case F_SETFL:
            current_task->fd[fd].flags |= arg;
            return 0;
        default:
            errno = ENOSYS;
            return -1;
    }

    return 0;
});
