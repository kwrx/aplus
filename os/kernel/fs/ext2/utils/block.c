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
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/mount.h>

#include "../ext2.h"



__thread_safe
void ext2_utils_read_block(ext2_t* ext2, uint32_t* block, uint32_t offset, void* data, size_t size) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(size);


    if(*block == 0)
        ext2_utils_alloc_block(ext2, block);


    if(*block > ext2->sb.s_blocks_count)
        kpanic("%s() FAIL! block(%d) > s_blocks_count(%d)", __func__, *block, ext2->sb.s_blocks_count);

    if(*block < ext2->first_block_group - 1)
        kpanic("%s() FAIL! block(%d) < first_block_group(%d) - 1", __func__, *block, ext2->first_block_group);

    if(vfs_read(ext2->dev, data, ((*block) * ext2->blocksize) + offset, size) != size)
        kpanic("%s() FAIL! vfs_read() %s", __func__, strerror(errno));
        
}


__thread_safe
void ext2_utils_write_block(ext2_t* ext2, uint32_t* block, uint32_t offset, void* data, size_t size) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(size);


    if(*block == 0)
        ext2_utils_alloc_block(ext2, block);


    if(*block > ext2->sb.s_blocks_count)
        kpanic("%s() FAIL! block(%d) > s_blocks_count(%d)", __func__, *block, ext2->sb.s_blocks_count);

    if(*block < ext2->first_block_group - 1)
        kpanic("%s() FAIL! block(%d) < first_block_group(%d) - 1", __func__, *block, ext2->first_block_group);

    if(vfs_write(ext2->dev, data, ((*block) * ext2->blocksize) + offset, size) != size)
        kpanic("%s() FAIL! vfs_read() %s", __func__, strerror(errno));

}



void ext2_utils_alloc_block(ext2_t* ext2, uint32_t* block) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);


    *block = 0;


    if(ext2->sb.s_free_blocks_count == 0)
        return;



    __lock(&ext2->lock, {

        
        int i;
        for(i = 0; i < (ext2->sb.s_blocks_count / ext2->sb.s_blocks_per_group); i++) {

            struct ext2_group_desc d;
            ext2_utils_read_block(ext2, ext2->first_block_group, i * sizeof(d), &d, sizeof(d));

                    
            if(d.bg_free_blocks_count == 0)
                continue;
            


            uint32_t* bitmap = ext2->cache;

            ext2_utils_read_block(ext2, d.bg_block_bitmap, 0, ext2->cache, ext2->blocksize);


                
            for(int j = 0; j < (ext2->blocksize / sizeof(uint32_t)); j++, bitmap++) {
                
                if(*bitmap == 0xFFFFFFFF)
                    continue;
            
                
                uint32_t b;
                b = __builtin_ffs(~(*bitmap)) - 1;
                b += j * 32;
                b += i * ext2->sb.s_blocks_per_group;

                *block = b;
                break;
            }



            ext2->sb.s_free_blocks_count--;
            d.bg_free_blocks_count--;
            
                
            ext2_utils_write_block(ext2, d.bg_block_bitmap, 0, ext2->cache, ext2->blocksize);
            ext2_utils_write_block(ext2, ext2->first_block_group, i * sizeof(d), &d, sizeof(d));

            break;

        }
        

    });


    DEBUG_ASSERT(*block != 0);

}


void ext2_utils_free_block(ext2_t* ext2, uint32_t block) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block > 1);




}
