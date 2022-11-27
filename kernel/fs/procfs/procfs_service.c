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
#include <stdio.h>
#include <sys/types.h>
#include <sys/mount.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>

#include "procfs.h"


static ino64_t __next_proc_ino = 1;



static ssize_t procfs_service_read(inode_t* inode, void* buf, off_t pos, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);

    if(unlikely(size == 0))
        return 0;


    procfs_service_t* service = inode->userdata;

    
    size_t max = 0;
    char* data = NULL;

    if(service->fetch(inode, &data, &max, service->arg) < 0)
        return errno = EIO, -1;


    DEBUG_ASSERT(data);
    DEBUG_ASSERT(max > 0);

    if(unlikely(pos >= max))
        return 0;

    if(unlikely(pos + size > max))
        size = max - pos;

    return memcpy(buf, data + pos, size)
         , size;

}


int procfs_service_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);

    DEBUG_ASSERT(st);

    procfs_service_t* service = inode->userdata;


    memset(st, 0, sizeof(struct stat));

    st->st_dev     = 0;
    st->st_ino     = inode->ino;
    st->st_mode    = service->mode;
    st->st_nlink   = 1;
    st->st_uid     = 0;
    st->st_gid     = 0;
    st->st_rdev    = 0;
    st->st_size    = 0;
    st->st_blksize = 1;
    st->st_blocks  = 0;
    st->st_atime   = arch_timer_gettime();
    st->st_mtime   = arch_timer_gettime();
    st->st_ctime   = arch_timer_gettime();

    return 0;

}

inode_t* procfs_service_inode(inode_t* parent, char* name, mode_t mode, int (*fetch) (inode_t*, char** buf, size_t*, void*), void* arg) {
    
    inode_t* inode = kcalloc(sizeof(inode_t), 1, GFP_KERNEL);

    inode->ino    = __next_proc_ino++;
    inode->parent = parent;
    inode->sb     = parent->sb;
    inode->flags |= INODE_FLAGS_DCACHE_DISABLED;

    strncpy(inode->name, name, CONFIG_MAXNAMLEN);


    procfs_service_t* service = kcalloc(sizeof(procfs_service_t), 1, GFP_KERNEL);

    service->fetch = fetch;
    service->arg   = arg;
    service->mode  = mode;

    inode->userdata = service;


    inode->ops.read    = procfs_service_read;
    inode->ops.getattr = procfs_service_getattr;

    spinlock_init(&inode->lock);


    return inode;

}