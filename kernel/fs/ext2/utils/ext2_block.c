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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mount.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>

#include "../ext2.h"




void ext2_utils_read_block(ext2_t* ext2, uint32_t block, uint32_t offset, void* data, size_t size) {
    
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(size);


    if(block > ext2->sb.s_blocks_count)
        kpanicf("%s() FAIL! block(%d) > s_blocks_count(%d)", __func__, block, ext2->sb.s_blocks_count);

    if(block < ext2->first_block_group - 1)
        kpanicf("%s() FAIL! block(%d) < first_block_group(%d) - 1", __func__, block, ext2->first_block_group);

    if(vfs_read(ext2->dev, data, ((block) * ext2->blocksize) + offset, size) != size)
        kpanicf("%s() FAIL! vfs_read() errno(%s)", __func__, strerror(errno));
        
}



void ext2_utils_write_block(ext2_t* ext2, uint32_t block, uint32_t offset, const void* data, size_t size) {
    
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(size);


    if(block > ext2->sb.s_blocks_count)
        kpanicf("%s() FAIL! block(%d) > s_blocks_count(%d)", __func__, block, ext2->sb.s_blocks_count);

    if(block < ext2->first_block_group - 1)
        kpanicf("%s() FAIL! block(%d) < first_block_group(%d) - 1", __func__, block, ext2->first_block_group);

    if(vfs_write(ext2->dev, data, ((block) * ext2->blocksize) + offset, size) != size)
        kpanicf("%s() FAIL! vfs_read() errno(%s)", __func__, strerror(errno));

}


void ext2_utils_zero_block(ext2_t* ext2, uint32_t block) {

    uint8_t zero[ext2->blocksize];
    memset(zero, 0, sizeof(zero));

    ext2_utils_write_block(ext2, block, 0, zero, sizeof(zero));

}



void ext2_utils_alloc_block(ext2_t* ext2, uint32_t* block) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);


    *block = 0;


    if(ext2->sb.s_free_blocks_count == 0)
        return;


    __lock(&ext2->lock, {

        size_t i;
        size_t j;

        for(i = 0; i < (ext2->sb.s_blocks_count / ext2->sb.s_blocks_per_group); i++) {

            struct ext2_group_desc d;
            ext2_utils_read_block(ext2, ext2->first_block_group, i * sizeof(d), &d, sizeof(d));

            if(d.bg_free_blocks_count == 0)
                continue;
            

            ext2_utils_read_block(ext2, d.bg_block_bitmap, 0, ext2->cache, ext2->blocksize);


                
            uint32_t* bitmap = ext2->cache;

            for(j = 0; j < (ext2->blocksize / sizeof(*bitmap)); j++, bitmap++) {
                
                if(*bitmap == 0xFFFFFFFF)
                    continue;
            
                
                uint32_t b, q;
                b = q = __builtin_ffs(~(*bitmap)) - 1;
                b += j * (sizeof(*bitmap) << 3);
                b += i * ext2->sb.s_blocks_per_group;

                *bitmap |= (1LL << q);
                *block = b;

                break;

            }


            DEBUG_ASSERT(block); 
            DEBUG_ASSERT(*block); 

            ext2->sb.s_free_blocks_count--;

            d.bg_free_blocks_count--;
            

            ext2_utils_zero_block(ext2, *block);
                
            ext2_utils_write_block(ext2, d.bg_block_bitmap, 0, ext2->cache, ext2->blocksize);
            ext2_utils_write_block(ext2, ext2->first_block_group, i * sizeof(d), &d, sizeof(d));


#if DEBUG_LEVEL_TRACE
            kprintf("ext2: alloc block %d (group %ld, bitmap %d, offset %ld)\n", *block, i, d.bg_block_bitmap, j);
#endif

            break;

        }
        

    });


    DEBUG_ASSERT(*block != 0);

}


void ext2_utils_free_block(ext2_t* ext2, uint32_t block) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block > 1);


    /* TODO: free block */


}



