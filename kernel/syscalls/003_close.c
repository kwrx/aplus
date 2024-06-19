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
 * Name:        close
 * Description: close a file descriptor
 * URL:         http://man7.org/linux/man-pages/man2/close.2.html
 *
 * Input Parameters:
 *  0: 0x03
 *  1: unsigned int fd
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(3, close,
long sys_close (unsigned int fd) {

#if defined(CONFIG_HAVE_NETWORK)

    if(unlikely(NETWORK_IS_SOCKFD(fd))) {
    
        ssize_t e;
        
        if((e = lwip_close(NETWORK_SOCKFD(fd)) < 0))
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


            __lock(&current_task->lock, {
                
                fd_remove(fds->descriptors[fd].ref, true);

                fds->descriptors[fd].ref = NULL;
                fds->descriptors[fd].flags = 0;
            
            });
            
        });


        return 0;

    }

});
