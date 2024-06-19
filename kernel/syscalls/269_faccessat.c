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
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus/hal.h>


extern long sys_newfstatat (int dfd, const char __user * filename, struct stat __user * statbuf, int flag);


/***
 * Name:        faccessat
 * Description: check user's permissions for a file
 * URL:         http://man7.org/linux/man-pages/man2/faccessat.2.html
 *
 * Input Parameters:
 *  0: 0x10d
 *  1: int dfd
 *  2: const char __user * filename
 *  3: int mode
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(269, faccessat,
long sys_faccessat (int dfd, const char __user * filename, int mode) {


    if(unlikely(!filename))
        return -EINVAL;

    if(unlikely(!uio_check(filename, R_OK)))
        return -EFAULT;


    int e;

    struct stat st;
    if((e = sys_newfstatat(dfd, filename, &st, 0)) < 0)
        return e;



    if(st.st_uid == current_task->uid) {
        if(!(
            (mode & R_OK ? st.st_mode & S_IRUSR : 1) &&
            (mode & W_OK ? st.st_mode & S_IWUSR : 1) &&
            (mode & X_OK ? st.st_mode & S_IXUSR : 1)
        )) return -EACCES;

    } else if(st.st_gid == current_task->gid) {
        if(!(
            (mode & R_OK ? st.st_mode & S_IRGRP : 1) &&
            (mode & W_OK ? st.st_mode & S_IWGRP : 1) &&
            (mode & X_OK ? st.st_mode & S_IXGRP : 1)
        )) return -EACCES;
    
    } else {
        if(!(
            (mode & R_OK ? st.st_mode & S_IROTH : 1) &&
            (mode & W_OK ? st.st_mode & S_IWOTH : 1) &&
            (mode & X_OK ? st.st_mode & S_IXOTH : 1)
        )) return -EACCES;

    }

    return 0;

});
