/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <aplus/hal.h>




/***
 * Name:        getdents
 * Description: get directory entries
 * URL:         http://man7.org/linux/man-pages/man2/getdents.2.html
 *
 * Input Parameters:
 *  0: 0x4e
 *  1: unsigned int fd
 *  2: struct linux_dirent __user * dirent
 *  3: unsigned int count
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */


SYSCALL(78, getdents,
long sys_getdents (unsigned int fd, struct dirent __user * dirent, unsigned int count) {
    
    if(unlikely(!dirent))
        return -EINVAL;

    if(unlikely(fd > CONFIG_OPEN_MAX - 1))
        return -EBADF;

    if(unlikely(!uio_check(dirent, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(count < sizeof(struct dirent)))
        return -EINVAL;

    
    
    DEBUG_ASSERT(current_task->fd[fd].ref);


    long e;

    __lock(&current_task->fd[fd].ref->lock,

        if((e = vfs_readdir(current_task->fd[fd].ref->inode, dirent, current_task->fd[fd].ref->position++, count / sizeof(struct dirent))) < 0)
            break;

        e *= sizeof(struct dirent);
        // FIXME
    );


    if(e < 0)
        return -errno;

    return e;
    
});
