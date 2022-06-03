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
#include <aplus/hal.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


/***
 * Name:        pipe2
 * Description: create pipe
 * URL:         http://man7.org/linux/man-pages/man2/pipe2.2.html
 *
 * Input Parameters:
 *  0: 0x125
 *  1: int __user * fildes
 *  2: int flags
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(293, pipe2,
long sys_pipe2 (int __user * fildes, int flags) {
    
    if(unlikely(!fildes))
        return -EFAULT;

    if(!uio_check(fildes, R_OK | W_OK))
        return -EFAULT;

    if(unlikely(flags & ~(O_CLOEXEC | O_NONBLOCK)))
        return -EINVAL;


    inode_t* inode;
    
    if((inode = vfs_mkfifo(CONFIG_BUFSIZ, flags)) == NULL)
        return -ENOMEM;



    struct file* ref;

    if((ref = fd_append(inode, 0, 0)) == NULL)
        return -ENFILE;

    fd_ref(ref);


    for(size_t i = 0; i < 2; i++) {

        int fd;

        __lock(&current_task->lock, {

            for(fd = 0; fd < CONFIG_OPEN_MAX; fd++) {

                if(!current_task->fd->descriptors[fd].ref)
                    break;

            }

            if(fd == CONFIG_OPEN_MAX)
                break;

            
            current_task->fd->descriptors[fd].ref = ref;
            current_task->fd->descriptors[fd].flags = i ? O_WRONLY : O_RDONLY;            

        });
        

        if(fd == CONFIG_OPEN_MAX)
            return -EMFILE;
            

        DEBUG_ASSERT(fd >= 0);
        DEBUG_ASSERT(fd <= CONFIG_OPEN_MAX - 1);
        DEBUG_ASSERT(current_task->fd->descriptors[fd].ref);
        DEBUG_ASSERT(current_task->fd->descriptors[fd].ref->inode);


        uio_w32(&fildes[i], fd);

    }


    return 0;

});
