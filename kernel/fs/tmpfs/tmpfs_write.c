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
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "tmpfs.h"



ssize_t tmpfs_write(inode_t* inode, const void* buf, off_t pos, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    if (unlikely(pos < 0))
        return -EINVAL;


    tmpfs_t* tmpfs   = (tmpfs_t*)inode->sb->fsinfo;
    tmpfs_inode_t* i = cache_get(&inode->sb->cache, inode->ino);


    if ((size_t)pos + len > (size_t)i->st.st_size) {

        size_t grow = ((size_t)pos + len) - (size_t)i->st.st_size;
        bool full   = false;

        scoped_lock(&tmpfs->lock) {

            if (((long)inode->sb->st.f_bavail - (long)grow) <= 0L) {
                full = true;
                break;
            }

            inode->sb->st.f_bfree -= grow;
            inode->sb->st.f_bavail -= grow;
        }

        if (unlikely(full))
            return -ENOSPC;


        if ((size_t)pos + len > i->capacity) {

            size_t capacity = (size_t)pos + len;
            capacity += capacity / 2;

            void* data = krealloc(i->data, capacity, GFP_USER);

            if (unlikely(!data)) {

                scoped_lock(&tmpfs->lock) {
                    inode->sb->st.f_bfree += grow;
                    inode->sb->st.f_bavail += grow;
                }

                return -ENOMEM;
            }

            i->data     = data;
            i->capacity = capacity;
        }

        if ((size_t)pos > (size_t)i->st.st_size)
            memset((void*)((uintptr_t)i->data + (uintptr_t)i->st.st_size), 0, (size_t)pos - (size_t)i->st.st_size);

        i->st.st_size = pos + len;
    }


    memcpy((void*)((uintptr_t)i->data + (uintptr_t)pos), buf, len);

    return len;
}
