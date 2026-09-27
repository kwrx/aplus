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



int tmpfs_truncate(inode_t* inode, off_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);



    if (unlikely(len < 0))
        return errno = EINVAL, -1;


    tmpfs_inode_t* i = cache_get(&inode->sb->cache, inode->ino);

    if (len >= i->st.st_size)
        return 0;


    void* data = krealloc(i->data, CONFIG_BUFSIZ + len, GFP_KERNEL);

    if (unlikely(!data))
        return errno = ENOMEM, -1;


    size_t freed = i->st.st_size - len;

    i->data       = data;
    i->capacity   = CONFIG_BUFSIZ + len;
    i->st.st_size = len;

    inode->sb->st.f_bfree += freed;
    inode->sb->st.f_bavail += freed;


    return 0;
}
