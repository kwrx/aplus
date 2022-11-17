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




ssize_t ext2_write(inode_t* inode, const void * buf, off_t pos, size_t len) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    ext2_t* ext2 = (ext2_t*) inode->sb->fsinfo;



    struct ext2_inode* n = cache_get(&inode->sb->cache, inode->ino);


    uint32_t* blocks = &n->i_block[0];
    uint32_t ib = pos / ext2->blocksize;
    uint32_t eb = (pos + len - 1) / ext2->blocksize;
    uint32_t off = 0;



    if(n->i_blocks < (pos + len) / ext2->blocksize + 1) {

        while(n->i_blocks < (pos + len) / ext2->blocksize + 1) {

            if(unlikely(inode->sb->st.f_bavail == 0))
                return errno = ENOSPC, -1;


            ext2_utils_alloc_inode_data(ext2, blocks, n->i_blocks++);

            inode->sb->st.f_bavail--;
            inode->sb->st.f_bfree--;
        
        }

        ext2_inode_set_size(ext2, n, pos + len);
        ext2_cache_sync(&inode->sb->cache, ext2, inode->ino, n);

    }
    


    if(pos % ext2->blocksize) {

        long p;
        p = ext2->blocksize - (pos % ext2->blocksize);
        p = p > len ? len : p;


        ext2_utils_write_inode_data(ext2, blocks, ib, pos % ext2->blocksize, buf, p);

        off += p;
        ib++;

    }


    if((pos + len) % ext2->blocksize && (ib <= eb)) {

        long p;
        p = (pos + len) % ext2->blocksize;


        ext2_utils_write_inode_data(ext2, blocks, eb, 0, (void*) ((uintptr_t) buf + len - p), p);

        eb--;
        
    }


    for(off_t i = eb - ib + 1; i > 0; i--, ib++, off += ext2->blocksize) {

        ext2_utils_write_inode_data(ext2, blocks, ib, 0, (void*) ((uintptr_t) buf + off), ext2->blocksize);

    }

    
    return len;
}