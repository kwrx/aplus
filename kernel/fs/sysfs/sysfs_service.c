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

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include "sysfs.h"


static ssize_t sysfs_service_read(inode_t* inode, void* buf, off_t pos, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_SYSFS);
    DEBUG_ASSERT(inode->userdata);

    if (unlikely(size == 0))
        return 0;

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;


    sysfs_service_t* service = inode->userdata;


    size_t max = 0;
    char* data = NULL;

    if (service->fetch(inode, &data, &max, service->arg) < 0)
        return errno = EIO, -1;

    DEBUG_ASSERT(data);

    if (unlikely((size_t)pos >= max))
        return 0;

    if (size > max - (size_t)pos)
        size = max - (size_t)pos;

    memcpy(buf, data + pos, size);

    return (ssize_t)size;
}

static ssize_t sysfs_service_readlink(inode_t* inode, char* buf, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_SYSFS);
    DEBUG_ASSERT(inode->userdata);

    sysfs_service_t* service = inode->userdata;

    size_t max = 0;
    char* data = NULL;

    if (service->fetch(inode, &data, &max, service->arg) < 0)
        return errno = EIO, -1;

    DEBUG_ASSERT(data);

    if (size > max)
        size = max;

    memcpy(buf, data, size);

    return (ssize_t)size;
}

static int sysfs_service_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_SYSFS);
    DEBUG_ASSERT(inode->userdata);

    DEBUG_ASSERT(st);

    sysfs_service_t* service = inode->userdata;


    memset(st, 0, sizeof(struct stat));

    time_t now = (time_t)arch_timer_gettime();

    st->st_dev   = 0;
    st->st_ino   = inode->ino;
    st->st_mode  = service->mode;
    st->st_nlink = S_ISDIR(service->mode) ? 2 : 1;
    st->st_uid   = 0;
    st->st_gid   = 0;
    st->st_rdev  = 0;
    st->st_size  = 0;

    if (S_ISLNK(service->mode)) {

        size_t max = 0;
        char* data = NULL;

        if (service->fetch(inode, &data, &max, service->arg) == 0)
            st->st_size = (off_t)max;
    }

    st->st_blksize = 1024;
    st->st_blocks  = 0;

    st->st_atime = now;
    st->st_mtime = now;
    st->st_ctime = now;

    return 0;
}

void sysfs_dirent(struct dirent* e, ino_t ino, unsigned char type, const char* name, off_t off) {

    e->d_ino    = ino;
    e->d_off    = off;
    e->d_type   = type;
    e->d_reclen = sizeof(struct dirent);

    strncpy(e->d_name, name, sizeof(e->d_name));
    e->d_name[sizeof(e->d_name) - 1] = '\0';
}


int sysfs_service_text_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)inode;

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(arg);

    *buf  = arg;
    *size = strlen(arg);

    return 0;
}


void sysfs_service_text_release(inode_t* inode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    sysfs_service_t* service = inode->userdata;

    kfree(service->arg);
    sysfs_service_release(inode);
}


void sysfs_service_release(inode_t* inode) {

    DEBUG_ASSERT(inode);

    kfree(inode->userdata);
    inode->userdata = NULL;
}

inode_t* sysfs_service_inode(inode_t* parent, const char* name, mode_t mode, int (*fetch)(inode_t*, char** buf, size_t*, void*), void* arg) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_SYSFS);
    DEBUG_ASSERT(name);


    inode_t* inode = kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    inode->ino    = 0;
    inode->parent = parent;
    inode->sb     = parent->sb;
    inode->flags |= INODE_FLAGS_DCACHE_DISABLED;

    strncpy(inode->name, name, CONFIG_MAXNAMLEN - 1);
    inode->name[CONFIG_MAXNAMLEN - 1] = '\0';


    sysfs_service_t* service = kcalloc(1, sizeof(sysfs_service_t), GFP_KERNEL);

    service->fetch = fetch;
    service->arg   = arg;
    service->mode  = mode;

    inode->userdata = service;


    if (S_ISREG(mode)) {
        inode->ops.read = sysfs_service_read;
    }

    if (S_ISLNK(mode)) {
        inode->ops.readlink = sysfs_service_readlink;
    }

    inode->ops.getattr = sysfs_service_getattr;

    spinlock_init(&inode->lock);


    return inode;
}
