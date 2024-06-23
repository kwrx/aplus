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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/utils/cache.h>
#include <aplus/vfs.h>

#include "tmpfs.h"



tmpfs_inode_t *tmpfs_cache_fetch(cache_t *cache, tmpfs_t *tmpfs, ino_t ino) {

    DEBUG_ASSERT(tmpfs);
    DEBUG_ASSERT(cache);

    tmpfs_inode_t *i = (tmpfs_inode_t *)kcalloc(sizeof(tmpfs_inode_t), 1, GFP_KERNEL);

    i->capacity = 0;
    i->data     = NULL;


    i->st.st_uid = current_task->uid;
    i->st.st_gid = current_task->gid;

    i->st.st_ino    = ino;
    i->st.st_nlink  = 1;
    i->st.st_blocks = 1;

    i->st.st_atime = arch_timer_gettime();
    i->st.st_mtime = arch_timer_gettime();
    i->st.st_ctime = arch_timer_gettime();

    return i;
}



void tmpfs_cache_commit(cache_t *cache, tmpfs_t *tmpfs, ino_t ino, tmpfs_inode_t *inode) {

    DEBUG_ASSERT(tmpfs);
    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(inode);

    __unused_param(ino);
}


void tmpfs_cache_release(cache_t *cache, tmpfs_t *tmpfs, ino_t ino, tmpfs_inode_t *inode) {

    DEBUG_ASSERT(tmpfs);
    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(inode);

    __unused_param(ino);

    if (inode->data) {
        kfree(inode->data);
    }

    kfree(inode);
}
