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



/**
 * @brief Tells whether a tmpfs directory still has an entry in it, with the tmpfs lock held.
 *
 * @param tmpfs The mounted tmpfs.
 * @param dir The directory.
 * @return true if some inode has @p dir as its parent.
 */
static bool __tmpfs_has_children(tmpfs_t* tmpfs, inode_t* dir) {

    list_each(tmpfs->children, i) {

        if (i->parent == dir)
            return true;
    }

    return false;
}


/**
 * @brief Removes an entry from a tmpfs directory; its data stays until the inode is released.
 *
 * @param inode The directory.
 * @param name The name of the entry.
 * @return 0 on success, or -1 with errno set.
 */
int tmpfs_unlink(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);

    DEBUG_ASSERT(name);



    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    int e          = ENOENT;

    scoped_lock(&tmpfs->lock) {

        inode_t* d = NULL;

        list_each(tmpfs->children, i) {

            if (likely(i->parent != inode))
                continue;

            if (likely(strcmp(i->name, name) != 0))
                continue;

            d = i;
            break;
        }

        if (!d)
            break;


        tmpfs_inode_t* ti = (tmpfs_inode_t*)cache_get(&inode->sb->cache, d->ino);

        if (S_ISDIR(ti->st.st_mode) && __tmpfs_has_children(tmpfs, d)) {
            e = ENOTEMPTY;
            break;
        }

        list_remove(tmpfs->children, d);

        ti->st.st_nlink = 0;

        inode->sb->st.f_ffree++;
        inode->sb->st.f_favail++;

        e = 0;
    }


    if (e)
        return errno = e, -1;

    return 0;
}


/**
 * @brief Frees the data of an unlinked tmpfs inode once nothing references it, returning its space.
 *
 * @param inode The inode.
 */
void tmpfs_release(inode_t* inode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);


    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    off_t size     = ((tmpfs_inode_t*)cache_get(&inode->sb->cache, inode->ino))->st.st_size;

    cache_remove(&inode->sb->cache, inode->ino);

    scoped_lock(&tmpfs->lock) {
        inode->sb->st.f_bavail += size;
        inode->sb->st.f_bfree += size;
    }
}
