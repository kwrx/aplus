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
#include <sys/mount.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include "procfs.h"



static ssize_t procfs_service_read(inode_t* inode, void* buf, off_t pos, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);

    if (unlikely(size == 0))
        return 0;

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;


    procfs_service_t* service = inode->userdata;


    /* The fetch renders into the shared scratch buffer, so the lock has to cover the copy
       out of it as well -- releasing it in between let another reader replace the contents
       before this one had read them. */
    scoped_lock(&procfs_scratch_lock) {

        size_t max  = 0;
        char* data  = NULL;

        if (service->fetch(inode, &data, &max, service->arg) < 0)
            return errno = EIO, -1;

        DEBUG_ASSERT(data);

        //? A zero-length /proc file is legal -- an empty boot cmdline, or the cmdline of a
        //? kernel thread. This used to be an assert that halted a debug kernel.
        if (unlikely((size_t)pos >= max))
            return 0;

        if (size > max - (size_t)pos)
            size = max - (size_t)pos;

        memcpy(buf, data + pos, size);

        return (ssize_t)size;
    }

    return 0;
}

static ssize_t procfs_service_readlink(inode_t* inode, char* buf, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);

    if (unlikely(size == 0))
        return 0;


    procfs_service_t* service = inode->userdata;


    scoped_lock(&procfs_scratch_lock) {

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

    return 0;
}

int procfs_service_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);

    DEBUG_ASSERT(st);

    procfs_service_t* service = inode->userdata;


    memset(st, 0, sizeof(struct stat));

    //? Sampled once: arch_timer_gettime() polls the RTC until two reads agree, and a tool
    //? walking /proc stats hundreds of paths a second.
    time_t now = (time_t)arch_timer_gettime();

    st->st_dev   = 0;
    st->st_ino   = inode->ino;
    st->st_mode  = service->mode;
    st->st_nlink = S_ISDIR(service->mode) ? 2 : 1;
    st->st_uid   = 0;
    st->st_gid   = 0;
    st->st_rdev  = 0;
    st->st_size  = 0;

    /* A symlink has to report its target's length: readlink(1) sizes its buffer from this,
       and with zero here it read nothing and printed an empty line. Regular /proc files
       keep reporting 0, as they do on Linux -- their content is generated per read. */
    if (S_ISLNK(service->mode) && service->fetch) {

        scoped_lock(&procfs_scratch_lock) {

            size_t max = 0;
            char* data = NULL;

            if (service->fetch(inode, &data, &max, service->arg) == 0)
                st->st_size = (off_t)max;
        }
    }

    /* 1024, not 1: stdio sizes its buffer from this, and a one-byte block size made it read
       a /proc file a byte at a time -- re-rendering the whole file for each one. */
    st->st_blksize = 1024;
    st->st_blocks  = 0;

    st->st_atime = now;
    st->st_mtime = now;
    st->st_ctime = now;

    return 0;
}

inode_t* procfs_service_inode(inode_t* parent, const char* name, mode_t mode, int (*fetch)(inode_t*, char** buf, size_t*, void*), void* arg) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(name);


    inode_t* inode = kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    //? The caller assigns a stable ino from the PROCFS_INO_* scheme; a bare global counter
    //? disagreed with what readdir advertised for the same file.
    inode->ino    = 0;
    inode->parent = parent;
    inode->sb     = parent->sb;
    inode->flags |= INODE_FLAGS_DCACHE_DISABLED;

    strncpy(inode->name, name, CONFIG_MAXNAMLEN - 1);
    inode->name[CONFIG_MAXNAMLEN - 1] = '\0';


    procfs_service_t* service = kcalloc(1, sizeof(procfs_service_t), GFP_KERNEL);

    service->fetch = fetch;
    service->arg   = arg;
    service->mode  = mode;

    inode->userdata = service;


    if (S_ISREG(mode)) {
        inode->ops.read = procfs_service_read;
    }

    if (S_ISLNK(mode)) {
        inode->ops.readlink = procfs_service_readlink;
    }

    inode->ops.getattr = procfs_service_getattr;

    spinlock_init(&inode->lock);


    return inode;
}


/* procfs_service_pid_to_task() lived here. It walked cpu->sched_queue with no lock and
   returned a bare task_t*, which a concurrent reaper could free before the caller touched
   it. Its replacements copy what they need out under cpu->sched_lock instead.
   @see procfs_task_snapshot(), procfs_pid_exists() in procfs_util.c. */
