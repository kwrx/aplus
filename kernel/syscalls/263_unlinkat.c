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
 * Name:        unlinkat
 * Description: delete a name and possibly the file it refers to
 * URL:         http://man7.org/linux/man-pages/man2/unlinkat.2.html
 *
 * Input Parameters:
 *  0: 0x107
 *  1: int dfd
 *  2: const char __user * pathname
 *  3: int flag
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(263, unlinkat,
long sys_unlinkat (int dfd, const char __user * pathname, int flags) {
    
    if(dfd < 0) {
       
        if(dfd != AT_FDCWD)
            return -EBADF;
    
    } else {

        if(dfd >= CONFIG_OPEN_MAX)
            return -EBADF;

        if(unlikely(!current_task->fd->descriptors[dfd].ref))
            return -EBADF;

    }


    if(unlikely(!pathname))
        return -EINVAL;
    
    if(unlikely(!uio_check(pathname, R_OK)))
        return -EFAULT;


    char __safe_pathname[CONFIG_PATH_MAX];
    uio_strncpy_u2s(__safe_pathname, pathname, CONFIG_PATH_MAX);


    inode_t* cwd;
    if(dfd == AT_FDCWD)
        cwd = current_task->fs->cwd;
    else
        cwd = current_task->fd->descriptors[dfd].ref->inode;


#if DEBUG_LEVEL_TRACE
    kprintf("unlinkat(%d, \"%s\", %d)\n", dfd, __safe_pathname, flags);
#endif


    inode_t* r;

    if((r = path_lookup(cwd, __safe_pathname, 0, 0)) == NULL)
        return -errno;



    struct stat st = { 0 };

    if(vfs_getattr(r, &st) < 0) {
        return (errno == ENOSYS) ? -EACCES : -errno;
    }


    if (
#ifdef O_NOFOLLOW
        !(flags & O_NOFOLLOW) &&
#endif
        S_ISLNK(st.st_mode)
    ) {
        if((r = path_follows(r, st.st_size)) == NULL)
            return -errno;
    }



#ifdef AT_REMOVEDIR

    if(S_ISDIR(st.st_mode) && !(flags & AT_REMOVEDIR))
        return -EISDIR;

#endif



    if(current_task->uid != 0) {

        if(st.st_uid == current_task->uid) {
            
            if(!(st.st_mode & S_IWUSR)) {
                return -EACCES;
            }

        } else if(st.st_gid == current_task->gid) {
            
            if(!(st.st_mode & S_IWGRP)) {
                return -EACCES;
            }

        } else {
            
            if(!(st.st_mode & S_IWOTH)) {
                return -EACCES;
            }
        
        }
    
    }


    DEBUG_ASSERT(r);
    DEBUG_ASSERT(r->parent);

    if((vfs_unlink(r->parent, r->name)) < 0) {
     
        if(errno != ENOSYS)
            return -errno;

        return -EPERM;
    
    }


    return 0;

});
