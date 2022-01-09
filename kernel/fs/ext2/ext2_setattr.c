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
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <sys/mount.h>

#include "ext2.h"



int ext2_setattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == EXT2_ID);
    DEBUG_ASSERT(st);

    struct ext2_inode* n = vfs_cache_get(&inode->sb->cache, inode->ino);

    n->i_mode  = st->st_mode;
    n->i_uid   = st->st_uid;
    n->i_gid   = st->st_gid;
    n->i_atime = st->st_atime;
    n->i_ctime = st->st_ctime;
    n->i_mtime = st->st_mtime;

    if(inode->sb->flags & MS_SYNCHRONOUS)
        ext2_cache_flush(&inode->sb->cache, inode->ino, n);

    return 0;

}