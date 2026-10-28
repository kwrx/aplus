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
 * @brief Moves an entry of a tmpfs directory to a new name, possibly in another directory of the same tmpfs.
 *
 * @param inode The directory holding the entry.
 * @param name The current name of the entry.
 * @param into The directory the entry moves to.
 * @param newname The new name.
 * @return 0 on success, -1 with errno set.
 */
int tmpfs_rename(inode_t* inode, const char* name, inode_t* into, const char* newname) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);
    DEBUG_ASSERT(into);
    DEBUG_ASSERT(into->sb == inode->sb);

    DEBUG_ASSERT(name);
    DEBUG_ASSERT(newname);



    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    inode_t* d     = NULL;

    scoped_lock(&tmpfs->lock) {

        list_each(tmpfs->children, i) {

            if (likely(i->parent != inode))
                continue;

            if (likely(strcmp(i->name, name) != 0))
                continue;

            d = i;
            break;
        }

        if (d) {
            strncpy(d->name, newname, CONFIG_MAXNAMLEN);
            d->parent = into;
        }
    }

    if (!d) {
        return errno = ENOENT, -1;
    }

    return 0;
}
