/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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
#include <aplus/vfs.h>
#include <aplus/vmobj.h>

#include "tmpfs.h"



struct vmobj* tmpfs_mmap(inode_t* inode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);


    tmpfs_inode_t* i = cache_get(&inode->sb->cache, inode->ino);

    scoped_lock(&i->lock) {

        if (!i->obj && !(i->obj = vmobj_create((size_t)i->st.st_size))) {
            return errno = ENOMEM, NULL;
        }

        vmobj_get(i->obj);

        return i->obj;
    }

    return errno = EIO, NULL;
}
