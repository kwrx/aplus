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
#include <aplus/vfs.h>

#include "iso9660.h"


int iso9660_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_ISO9660);
    DEBUG_ASSERT(inode->sb->dev);

    DEBUG_ASSERT(st);


    if (unlikely(inode == inode->sb->root)) {

        st->st_dev     = inode->sb->dev->ino;
        st->st_ino     = inode->ino;
        st->st_mode    = S_IFDIR | 0555;
        st->st_nlink   = 1;
        st->st_uid     = 0;
        st->st_gid     = 0;
        st->st_rdev    = 0;
        st->st_size    = 0;
        st->st_blksize = ISO9660_BLOCK_SIZE;
        st->st_blocks  = 0;
        st->st_atime   = arch_timer_gettime();
        st->st_mtime   = arch_timer_gettime();
        st->st_ctime   = arch_timer_gettime();

    } else {


        iso9660_inode_t* i = cache_get(&inode->sb->cache, inode->userdata);

        memcpy(st, &i->st, sizeof(struct stat));
    }

    return 0;
}
