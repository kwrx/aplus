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
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


/***
 * Name:        pipe2
 * Description: create pipe
 * URL:         http://man7.org/linux/man-pages/man2/pipe2.2.html
 *
 * Input Parameters:
 *  0: 0x125
 *  1: int  * fildes
 *  2: int flags
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    293, pipe2, long sys_pipe2(int* fildes, int flags) {
        if (unlikely(!fildes))
            return -EFAULT;

        if (!uio_check(fildes, R_OK | W_OK))
            return -EFAULT;

        if (unlikely(flags & ~(O_CLOEXEC | O_NONBLOCK)))
            return -EINVAL;


        inode_t* inode = pipefs_inode();

        if ((inode = vfs_mkfifo(inode, CONFIG_PIPESIZ, flags)) == NULL)
            return -ENOMEM;



        struct file* refs[2];

        if ((refs[0] = fd_append(inode, 0, 0)) == NULL)
            return -ENFILE;

        if ((refs[1] = fd_append(inode, 0, 0)) == NULL)
            return -ENFILE;


        shared_ptr_access(current_task->fd, fds, {
            for (size_t i = 0; i < 2; i++) {

                int fd = -1;

                scoped_lock(&current_task->lock) {
                    for (fd = 0; fd < CONFIG_OPEN_MAX; fd++) {

                        if (!fds->descriptors[fd].ref)
                            break;
                    }

                    if (fd == CONFIG_OPEN_MAX)
                        break;


                    fds->descriptors[fd].ref   = refs[i];
                    fds->descriptors[fd].flags = (flags & O_NONBLOCK) | (flags & O_CLOEXEC) | (i ? O_WRONLY : O_RDONLY);
                }


                if (fd == CONFIG_OPEN_MAX)
                    return -EMFILE;


                DEBUG_ASSERT(fd >= 0);
                DEBUG_ASSERT(fd <= CONFIG_OPEN_MAX - 1);
                DEBUG_ASSERT(fds->descriptors[fd].ref);
                DEBUG_ASSERT(fds->descriptors[fd].ref->inode);


#if DEBUG_LEVEL_TRACE
                kprintf("pipe2: assign fd[%zd] = %d (flags: %o)\n", i, fd, flags);
#endif


                uio_w32(&fildes[i], fd);
            }
        });


        return 0;
    });
