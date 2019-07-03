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
int ext2_read(inode_t* inode, void * buf, off_t pos, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->ino);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsinfo);
    DEBUG_ASSERT(inode->sb->fsid == EXT2_ID);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    ext2_t* ext2 = (ext2_t*) inode->sb->fsinfo;


    if(pos + len > inode->ino->st.st_size)
        len = inode->ino->st.st_size - pos;

    DEBUG_ASSERT(len >= 0);



    // Cache Blocks
    if(inode->ino->userdata == NULL) {

        struct ext2_inode node;
        ext2_utils_read_inode(ext2, inode->ino->st.st_ino, &node);

        inode->ino->userdata = kcalloc(EXT2_N_BLOCKS, sizeof(uint32_t), GFP_KERNEL);
        memcpy(inode->ino->userdata, node.i_block, EXT2_N_BLOCKS * sizeof(uint32_t));

    }

    DEBUG_ASSERT(inode->ino->userdata);

    uint32_t* blocks = (uint32_t*) inode->ino->userdata;


    uint32_t sb = pos / ext2->blocksize;
    uint32_t eb = (pos + len - 1) / ext2->blocksize;
    uint32_t off = 0;



    if(pos % ext2->blocksize) {
        long p;
        p = ext2->blocksize - (pos % ext2->blocksize);
        p = p > len ? len : p;


        ext2_utils_read_inode_data(ext2, blocks, sb, pos % ext2->blocksize, buf, p);

        off += p;
        sb++;
    }


    if((pos + len) % ext2->blocksize && (sb <= eb)) {
        long p;
        p = (pos + len) % ext2->blocksize;


        ext2_utils_read_inode_data(ext2, blocks, eb, 0, (void*) ((uintptr_t) buf + len - p), p);

        eb--;
    }



    long i = eb - sb + 1;

    for(; i > 0; i--, sb++, off += ext2->blocksize)
        ext2_utils_read_inode_data(ext2, blocks, sb, 0, (void*) ((uintptr_t) buf + off), ext2->blocksize);

    
    return len;
}