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

#include "ext2.h"



int ext2_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(st);

    struct ext2_inode* n = cache_get(&inode->sb->cache, inode->ino);

    struct stat ds;
    if (vfs_getattr(inode->sb->dev, &ds) < 0) {
        return -1;
    }

    dev_t rdev = 0;
    if (S_ISCHR(n->i_mode) || S_ISBLK(n->i_mode)) {
        if (n->i_block[0]) {
            rdev = makedev((n->i_block[0] >> 8) & 0xFF, n->i_block[0] & 0xFF);
        } else {
            rdev = makedev((n->i_block[1] & 0xFFF00) >> 8, (n->i_block[1] & 0xFF) | ((n->i_block[1] >> 12) & 0xFFF00));
        }
    }

    st->st_dev     = ds.st_rdev;
    st->st_rdev    = rdev;
    st->st_ino     = inode->ino;
    st->st_mode    = n->i_mode;
    st->st_nlink   = n->i_links_count;
    st->st_uid     = n->i_uid | ((uid_t)n->osd2.linux2.l_i_uid_high << 16);
    st->st_gid     = n->i_gid | ((gid_t)n->osd2.linux2.l_i_gid_high << 16);
    st->st_size    = ext2_inode_get_size(inode->sb->fsinfo, n);
    st->st_blksize = inode->sb->st.f_bsize;
    st->st_blocks  = n->i_blocks;
    st->st_atime   = n->i_atime;
    st->st_ctime   = n->i_ctime;
    st->st_mtime   = n->i_mtime;

    return 0;
}
