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

#include <aplus/utils/list.h>

#include "tmpfs.h"



int tmpfs_unlink(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);

    DEBUG_ASSERT(name);



    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    inode_t* d     = NULL;


    list_each(tmpfs->children, i) {

        if (likely(i->parent != inode))
            continue;

        if (likely(strcmp(i->name, name) != 0))
            continue;

        d = i;
        break;
    }

    if (!d) {
        return errno = ENOENT, -1;
    }

    list_remove(tmpfs->children, d);



    tmpfs_inode_t* i = cache_get(&inode->sb->cache, d->ino);

    inode->sb->st.f_ffree++;
    inode->sb->st.f_favail++;
    inode->sb->st.f_bavail += i->st.st_size;
    inode->sb->st.f_bfree += i->st.st_size;

    cache_remove(&inode->sb->cache, d->ino);

    return 0;
}
