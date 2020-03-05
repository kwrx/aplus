/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <stdint.h>
#include <aplus/errno.h>

#include "ext2.h"



int ext2_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == EXT2_ID);
    DEBUG_ASSERT(st);

    struct ext2_inode* n = vfs_cache_get(&inode->sb->cache, inode->ino);


    st->st_dev = 0; /* FIXME */
    st->st_ino = inode->ino;
    st->st_mode = n->i_mode;
    st->st_nlink = n->i_links_count;
    st->st_uid = n->i_uid;
    st->st_gid = n->i_gid;
    st->st_rdev = 0; /* FIXME */
    st->st_size = n->i_size;
    st->st_blksize = 512;
    st->st_blocks = n->i_blocks;
    st->st_atime = n->i_atime;
    st->st_ctime = n->i_mtime;
    st->st_mtime = n->i_ctime;
    

    if(sizeof(off_t) == 8) {
        
        ext2_t* ext2 = (ext2_t*) inode->sb->fsinfo;

        if(ext2->sb.s_rev_level == EXT2_DYNAMIC_REV)
            st->st_size |= (off_t) ((uint64_t) n->i_size_high << 32);

    }


    return 0;
}