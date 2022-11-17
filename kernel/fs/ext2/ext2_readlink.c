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

#include "ext2.h"



ssize_t ext2_readlink(inode_t* inode, char * buf, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsinfo);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    ext2_t* ext2 = (ext2_t*) inode->sb->fsinfo;



    struct ext2_inode* n = cache_get(&inode->sb->cache, inode->ino);

    if(unlikely(len > n->i_size)) {
        len = n->i_size;
    }
    
    
    //! WARN
    DEBUG_ASSERT(n->i_size <= ext2->blocksize);

    if(len < 60) {

        memcpy(buf, (const char*) n->i_block, len);

    } else {

        ext2_utils_read_inode_data(ext2, n->i_block, 0, 0, buf, len);

    }

    return len;
    
}