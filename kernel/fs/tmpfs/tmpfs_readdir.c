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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>
#include <stdint.h>

#include <aplus/utils/list.h>

#include "tmpfs.h"


static inode_t** __next_entry(list(inode_t*, children), inode_t* parent, inode_t** curr) {

    if (curr == NULL) {

        curr = list_elem_front(children);

    } else {

        curr = list_elem_next(curr);
    }

    while (curr && (*curr)->parent != parent) {

        curr = list_elem_next(curr);

        if (curr == NULL)
            return NULL;
    }

    return curr;
}


static ssize_t __tmpfs_readdir_locked(tmpfs_t* tmpfs, inode_t* inode, struct dirent* e, off_t pos, size_t count, const struct stat* self, const struct stat* parent) {

    size_t i = 0;


    if (pos == 0 && i < count) {

        e[i].d_ino    = inode->ino;
        e[i].d_off    = 1;
        e[i].d_reclen = sizeof(struct dirent);
        e[i].d_type   = MODE_2_DIRENT_TYPE(self->st_mode);

        strncpy(e[i].d_name, ".", sizeof(e[i].d_name));

        i++;
    }

    if (pos <= 1 && i < count) {

        e[i].d_ino    = inode->parent->ino;
        e[i].d_off    = 2;
        e[i].d_reclen = sizeof(struct dirent);
        e[i].d_type   = MODE_2_DIRENT_TYPE(parent->st_mode);

        strncpy(e[i].d_name, "..", sizeof(e[i].d_name));

        i++;
    }


    for (inode_t** entry = __next_entry(tmpfs->children, inode, NULL); entry && i < count; entry = __next_entry(tmpfs->children, inode, entry)) {

        uint64_t seq = ((tmpfs_entry_t*)*entry)->seq;

        if ((off_t)seq + 2 <= pos)
            continue;


        tmpfs_inode_t* c = cache_get(&inode->sb->cache, (*entry)->ino);

        e[i].d_ino    = c->st.st_ino;
        e[i].d_off    = (off_t)seq + 2;
        e[i].d_reclen = sizeof(struct dirent);
        e[i].d_type   = MODE_2_DIRENT_TYPE(c->st.st_mode);

        strncpy(e[i].d_name, (*entry)->name, sizeof(e[i].d_name));

        i++;
    }

    return i;
}


ssize_t tmpfs_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsinfo);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);
    DEBUG_ASSERT(inode->parent);

    DEBUG_ASSERT(e);

    if (unlikely(count == 0))
        return 0;


    struct stat self   = {0};
    struct stat parent = {0};

    if (pos <= 0 && inode->ops.getattr)
        inode->ops.getattr(inode, &self);

    if (pos <= 1 && pos + (off_t)count > 1 && inode->parent->ops.getattr)
        inode->parent->ops.getattr(inode->parent, &parent);


    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    ssize_t n      = 0;

    scoped_lock(&tmpfs->lock) {
        n = __tmpfs_readdir_locked(tmpfs, inode, e, pos, count, &self, &parent);
    }

    return n;
}
