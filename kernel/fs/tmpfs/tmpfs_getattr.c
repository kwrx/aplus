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
#include <sys/sysmacros.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "tmpfs.h"


int tmpfs_getattr(inode_t *inode, struct stat *st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);

    DEBUG_ASSERT(st);


    if (inode == inode->sb->root) {

        memset(st, 0, sizeof(struct stat));

        st->st_ino     = inode->ino;
        st->st_mode    = S_IFDIR | 0755;
        st->st_dev     = makedev(0, 34);
        st->st_nlink   = 2;
        st->st_uid     = 0;
        st->st_gid     = 0;
        st->st_rdev    = 0;
        st->st_size    = 0;
        st->st_blksize = 0;
        st->st_blocks  = 0;
        st->st_atime   = arch_timer_gettime();
        st->st_mtime   = arch_timer_gettime();
        st->st_ctime   = arch_timer_gettime();

    } else {

        tmpfs_inode_t *i = cache_get(&inode->sb->cache, inode->ino);

        if (unlikely(!i))
            return -ENOENT;

        memcpy(st, &i->st, sizeof(struct stat));
    }

    return 0;
}
