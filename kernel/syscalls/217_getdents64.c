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
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/errno.h>




struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char d_name[0];
};


/***
 * Name:        getdents64
 * Description: get directory entries
 * URL:         http://man7.org/linux/man-pages/man2/getdents64.2.html
 *
 * Input Parameters:
 *  0: 0xd9
 *  1: unsigned int fd
 *  2: struct linux_dirent64 __user * dirent
 *  3: unsigned int count
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(217, getdents64,
long sys_getdents64 (unsigned int fd, struct linux_dirent64 __user * dirent, unsigned int count) {
        
    if(unlikely(!dirent))
        return -EINVAL;

    if(unlikely(fd > CONFIG_OPEN_MAX))
        return -EBADF;

    if(unlikely(!uio_check(dirent, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(count < sizeof(struct dirent)))
        return -EINVAL;

    
    
    DEBUG_ASSERT(current_task->fd->descriptors[fd].ref);


    ssize_t e = 0;
    ssize_t r = 0;

    __lock(&current_task->fd->descriptors[fd].ref->lock,


        int i;
        for(i = 0; i + sizeof(struct linux_dirent64) < count; ) {

            struct dirent ent;

            if((e = vfs_readdir (current_task->fd->descriptors[fd].ref->inode, &ent, current_task->fd->descriptors[fd].ref->position++, 1)) <= 0)
                break;


            size_t reclen = sizeof(struct linux_dirent64) + strlen(ent.d_name);

            uio_w64(&dirent->d_ino, ent.d_ino);
            uio_w64(&dirent->d_off, i);
            uio_w16(&dirent->d_reclen, reclen);
            uio_w8 (&dirent->d_type, ent.d_type);

            uio_strcpy_s2u(&dirent->d_name[0], ent.d_name);
            

            r += reclen;
            i += reclen;

            dirent = (struct linux_dirent64*) ((uintptr_t) dirent + reclen);

        }
        
    );


    if(e < 0)
        return -errno;

    return r;
    
});
