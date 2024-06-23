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

#include <aplus/utils/cache.h>

#include "procfs.h"


static cache_t cache = {0};


static inode_t* procfs_service_pid_finddir(inode_t* inode, const char* name) {
    return NULL;
}

static ssize_t procfs_service_pid_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {
    return 0;
}

static int procfs_service_pid_self_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->sb->root == inode);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    static char buffer[64] = {0};

    *size = snprintf(buffer, sizeof(buffer), "%d", current_task->tid);

    *buf = buffer;

    return 0;
}


static inode_t* procfs_service_pid_cache_fetch(cache_t* cache, inode_t* parent, cache_key_t key) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(parent);


    inode_t* inode = NULL;


    pid_t pid = (pid_t)((uintptr_t)key);

    if (pid > 0) {

        char name[64] = {0};
        snprintf(name, sizeof(name), "%d", pid);

        inode = procfs_service_inode(parent, name, S_IFDIR | 0555, NULL, key);

        inode->ops.finddir = procfs_service_pid_finddir;
        inode->ops.readdir = procfs_service_pid_readdir;

    } else {

        inode = procfs_service_inode(parent, "self", S_IFLNK | 0555, procfs_service_pid_self_fetch, key);
    }

    return inode;
}

static void procfs_service_pid_cache_commit(cache_t* cache, inode_t* parent, cache_key_t key, inode_t* value) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(value);
    DEBUG_ASSERT(parent);
}

static void procfs_service_pid_cache_release(cache_t* cache, inode_t* parent, cache_key_t key, inode_t* value) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(value);
    DEBUG_ASSERT(parent);

    kfree(value);
}



inode_t* procfs_service_pid_inode(inode_t* parent, pid_t pid) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_PROCFS);

    return cache_get(&cache, pid);
}

void procfs_service_pid_init(inode_t* parent) {

    cache_ops_t ops = {
        .fetch   = (cache_fetch_handler_t)procfs_service_pid_cache_fetch,
        .commit  = (cache_commit_handler_t)procfs_service_pid_cache_commit,
        .release = (cache_release_handler_t)procfs_service_pid_cache_release,
    };

    cache_init(&cache, &ops, SIZE_MAX, parent);
}
