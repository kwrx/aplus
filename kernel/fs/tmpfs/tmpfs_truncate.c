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
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "tmpfs.h"



/**
 * @brief Extends a tmpfs file to @p len bytes, the new ones reading as zeros.
 *
 * @param inode The inode, whose lock the caller holds.
 * @param i The file's tmpfs data.
 * @param len The new size, larger than the current one.
 * @return 0, or -1 with errno ENOSPC or ENOMEM.
 */
static int tmpfs_truncate_grow(inode_t* inode, tmpfs_inode_t* i, off_t len) {

    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    size_t grow    = (size_t)len - (size_t)i->st.st_size;
    bool full      = false;

    scoped_lock(&tmpfs->lock) {

        if (((long)inode->sb->st.f_bavail - (long)grow) <= 0L) {
            full = true;
            break;
        }

        inode->sb->st.f_bfree -= grow;
        inode->sb->st.f_bavail -= grow;
    }

    if (unlikely(full))
        return errno = ENOSPC, -1;


    if ((size_t)len > i->capacity) {

        void* data = krealloc(i->data, (size_t)len, GFP_USER);

        if (unlikely(!data)) {

            scoped_lock(&tmpfs->lock) {
                inode->sb->st.f_bfree += grow;
                inode->sb->st.f_bavail += grow;
            }

            return errno = ENOMEM, -1;
        }

        i->data     = data;
        i->capacity = (size_t)len;
    }

    memset((void*)((uintptr_t)i->data + (uintptr_t)i->st.st_size), 0, grow);

    i->st.st_size = len;

    return 0;
}


int tmpfs_truncate(inode_t* inode, off_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);



    if (unlikely(len < 0))
        return errno = EINVAL, -1;


    tmpfs_inode_t* i = cache_get(&inode->sb->cache, inode->ino);

    if (len == i->st.st_size)
        return 0;

    if (len > i->st.st_size)
        return tmpfs_truncate_grow(inode, i, len);


    void* data = krealloc(i->data, CONFIG_BUFSIZ + len, GFP_KERNEL);

    if (unlikely(!data))
        return errno = ENOMEM, -1;


    size_t freed = i->st.st_size - len;

    i->data       = data;
    i->capacity   = CONFIG_BUFSIZ + len;
    i->st.st_size = len;

    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;

    scoped_lock(&tmpfs->lock) {
        inode->sb->st.f_bfree += freed;
        inode->sb->st.f_bavail += freed;
    }


    return 0;
}
