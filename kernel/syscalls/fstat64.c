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
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(197, fstat64,
int sys_fstat64(int fd, struct stat64* st) {
    if(unlikely(fd >= TASK_FD_COUNT || fd < 0)) {
        errno = EBADF;
        return -1;
    }

    inode_t* inode = current_task->fd[fd].inode;
    
    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }

    if(unlikely(!st)) {
        errno = EINVAL;
        return -1;
    }

    st->st_dev = inode->dev;
    st->st_ino = inode->ino;
    st->st_mode = inode->mode;
    st->st_nlink = inode->nlink;
    st->st_uid = inode->uid;
    st->st_gid = inode->gid;
    st->st_rdev = inode->rdev;
    st->st_size = (off64_t) inode->size;
    st->st_blksize = BUFSIZ;
    st->st_blocks = st->st_size / st->st_blksize;
    st->st_atim.tv_sec = inode->atime;
    st->st_mtim.tv_sec = inode->mtime;
    st->st_ctim.tv_sec = inode->ctime;
    st->st_atim.tv_nsec =
    st->st_mtim.tv_nsec =
    st->st_ctim.tv_nsec = 0;
    
    st->__st_dev_padding = 0;
    st->__st_rdev_padding = 0;
    st->__st_ino_truncated = (long) inode->ino;

    return 0;
});
