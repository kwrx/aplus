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
#include <aplus/errno.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mount.h>

#include "../ext2.h"





void ext2_utils_read_inode(ext2_t* ext2, ino_t ino, void* data) {
    
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(ino != EXT2_BAD_INO);
    DEBUG_ASSERT(ino != 0);
    
    if(ino > ext2->sb.s_inodes_count)
        kpanicf("%s() FAIL! ino(%d) > s_inodes_count(%d)", __func__, ino, ext2->sb.s_inodes_count);


    struct ext2_group_desc d;
    ext2_utils_read_block(ext2, ext2->first_block_group, ((ino - 1) / ext2->sb.s_inodes_per_group) * sizeof(d), &d, sizeof(d));
    ext2_utils_read_block(ext2, d.bg_inode_table,        ((ino - 1) % ext2->sb.s_inodes_per_group) * ext2->inodesize, data, ext2->inodesize);

}



void ext2_utils_write_inode(ext2_t* ext2, ino_t ino, void* data) {
    
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(ino != EXT2_BAD_INO);
    DEBUG_ASSERT(ino != 0);

    struct ext2_group_desc d;
    ext2_utils_read_block(ext2, ext2->first_block_group, ((ino - 1) / ext2->sb.s_inodes_per_group) * sizeof(d), &d, sizeof(d));
    ext2_utils_write_block(ext2, d.bg_inode_table,       ((ino - 1) % ext2->sb.s_inodes_per_group) * ext2->inodesize, data, ext2->inodesize);

}



void ext2_utils_read_inode_data(ext2_t* ext2, uint32_t* blocks, uint32_t block, uint32_t offset, void* data, size_t size) {
      
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(blocks);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(size <= ext2->blocksize);


    uint32_t a = EXT2_NDIR_BLOCKS
           , b = (ext2->blocksize / sizeof(uint32_t))
           ;


    // Direct Blocks
    if(block < a) {
    
        ext2_utils_read_block(ext2, blocks[block], offset, data, size);
        return;    
    
    }


    // Indirect Blocks
    block -= a;
    
    if(block < b) {

        ext2_utils_read_block(ext2, blocks[EXT2_IND_BLOCK], sizeof(uint32_t) * block, &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, offset, data, size);
        return;

    }


    // Double Indirect Blocks
    block -= b;

    if(block < (b * b)) {

        ext2_utils_read_block(ext2, blocks[EXT2_DIND_BLOCK], sizeof(uint32_t) * (block / b), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * (block % b), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, offset, data, size);

        return;

    }


    // Triply Indirect Blocks
    block -= b * b;

    if(block < (b * b * b)) {

        ext2_utils_read_block(ext2, blocks[EXT2_TIND_BLOCK], sizeof(uint32_t) * (block / (b * b)), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) / b), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) % b), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, offset, data, size);
        return;

    }


    kpanicf("%s() FAIL! block is too high!");

}





void ext2_utils_write_inode_data(ext2_t* ext2, uint32_t* blocks, uint32_t block, uint32_t offset, const void* data, size_t size) {
   
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(blocks);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(size <= ext2->blocksize);


    uint32_t a = EXT2_NDIR_BLOCKS
           , b = (ext2->blocksize / sizeof(uint32_t))
           ;


    // Direct Blocks
    if(block < a) {
        
        ext2_utils_write_block(ext2, blocks[block], offset, data, size);
        return;    
    
    }


    // Indirect Blocks
    block -= a;
    
    if(block < b) {

        ext2_utils_read_block(ext2, blocks[EXT2_IND_BLOCK], sizeof(uint32_t) * block, &block, sizeof(uint32_t));
        ext2_utils_write_block(ext2, block, offset, data, size);
        return;

    }


    // Double Indirect Blocks
    block -= b;

    if(block < (b * b)) {

        ext2_utils_read_block(ext2, blocks[EXT2_DIND_BLOCK], sizeof(uint32_t) * (block / b), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * (block % b), &block, sizeof(uint32_t));
        ext2_utils_write_block(ext2, block, offset, data, size);

        return;

    }


    // Triply Indirect Blocks
    block -= b * b;

    if(block < (b * b * b)) {

        ext2_utils_read_block(ext2, blocks[EXT2_TIND_BLOCK], sizeof(uint32_t) * (block / (b * b)), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) / b), &block, sizeof(uint32_t));
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) % b), &block, sizeof(uint32_t));
        ext2_utils_write_block(ext2, block, offset, data, size);
        return;

    }


    kpanicf("%s() FAIL! block is too high!");

}