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
#include <aplus/mm.h>
#include <stdint.h>
#include <errno.h>

#include "ext2.h"


__thread_safe
ssize_t ext2_readlink(inode_t* inode, char * buf, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == EXT2_ID);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    ext2_t* ext2 = (ext2_t*) inode->sb->fsinfo;



    struct ext2_inode* n = vfs_cache_get(&inode->sb->cache, inode->ino);

    if(len > n->i_size)
        len = n->i_size;
    

    ext2_utils_read_inode_data(ext2, n->i_block, 0, 0, buf, len);

    return len;
}