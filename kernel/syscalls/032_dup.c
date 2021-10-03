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
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


/***
 * Name:        dup
 * Description: duplicate a file descriptor
 * URL:         http://man7.org/linux/man-pages/man2/dup.2.html
 *
 * Input Parameters:
 *  0: 0x20
 *  1: unsigned int fildes
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(32, dup,
long sys_dup (unsigned int fd) {

    if(unlikely(fd > CONFIG_OPEN_MAX)) // TODO: add network support
        return -EBADF;

    if(unlikely(!current_task->fd->descriptors[fd].ref))
        return -EBADF;


    int i;

    __lock(&current_task->lock, {

        for(i = 0; i < CONFIG_OPEN_MAX; i++)
            if(!current_task->fd->descriptors[i].ref)
                break;

        if(i == CONFIG_OPEN_MAX)
            break;


        __lock(&current_task->fd->descriptors[fd].ref->lock, {
        
            current_task->fd->descriptors[i].ref = current_task->fd->descriptors[fd].ref;
            current_task->fd->descriptors[i].flags = current_task->fd->descriptors[fd].flags;

            current_task->fd->descriptors[i].ref->refcount++;

        });

    });
    

    if(i == CONFIG_OPEN_MAX)
        return -EMFILE;

    return i;

});
