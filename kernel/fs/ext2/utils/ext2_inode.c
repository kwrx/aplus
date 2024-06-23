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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include <stdint.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <unistd.h>

#include "../ext2.h"



void ext2_utils_read_inode(ext2_t* ext2, ino_t ino, void* data) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(ino != EXT2_BAD_INO);
    DEBUG_ASSERT(ino != 0);

    if (ino > ext2->sb.s_inodes_count)
        kpanicf("%s() FAIL! ino(%ld) > s_inodes_count(%d)", __func__, ino, ext2->sb.s_inodes_count);


    struct ext2_group_desc d;
    ext2_utils_read_block(ext2, ext2->first_block_group, ((ino - 1) / ext2->sb.s_inodes_per_group) * sizeof(d), &d, sizeof(d), true);
    ext2_utils_read_block(ext2, d.bg_inode_table, ((ino - 1) % ext2->sb.s_inodes_per_group) * ext2->inodesize, data, ext2->inodesize, false);
}



void ext2_utils_write_inode(ext2_t* ext2, ino_t ino, const void* data) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);
    DEBUG_ASSERT(ino != EXT2_BAD_INO);
    DEBUG_ASSERT(ino != 0);

    struct ext2_group_desc d;
    ext2_utils_read_block(ext2, ext2->first_block_group, ((ino - 1) / ext2->sb.s_inodes_per_group) * sizeof(d), &d, sizeof(d), true);
    ext2_utils_write_block(ext2, d.bg_inode_table, ((ino - 1) % ext2->sb.s_inodes_per_group) * ext2->inodesize, data, ext2->inodesize);
}



void ext2_utils_read_inode_data(ext2_t* ext2, uint32_t* blocks, uint32_t block, uint32_t offset, void* data, size_t size) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(blocks);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(size <= ext2->blocksize);


    uint32_t a = EXT2_NDIR_BLOCKS, b = (ext2->blocksize / sizeof(uint32_t));


    // Direct Blocks
    if (block < a) {

        ext2_utils_read_block(ext2, blocks[block], offset, data, size, false);
        return;
    }


    // Indirect Blocks
    block -= a;

    if (block < b) {

        ext2_utils_read_block(ext2, blocks[EXT2_IND_BLOCK], sizeof(uint32_t) * block, &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, offset, data, size, false);
        return;
    }


    // Double Indirect Blocks
    block -= b;

    if (block < (b * b)) {

        uint32_t p = block;

        ext2_utils_read_block(ext2, blocks[EXT2_DIND_BLOCK], sizeof(uint32_t) * (p / b), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * (p % b), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, offset, data, size, false);
        return;
    }


    // Triply Indirect Blocks
    block -= b * b;

    if (block < (b * b * b)) {

        uint32_t p = block;

        ext2_utils_read_block(ext2, blocks[EXT2_TIND_BLOCK], sizeof(uint32_t) * (p / (b * b)), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((p % (b * b)) / b), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((p % (b * b)) % b), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, offset, data, size, false);
        return;
    }


    kpanicf("%s() FAIL! block is too high!", __func__);
}



void ext2_utils_write_inode_data(ext2_t* ext2, uint32_t* blocks, uint32_t block, uint32_t offset, const void* data, size_t size) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(blocks);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(size <= ext2->blocksize);


    uint32_t a = EXT2_NDIR_BLOCKS, b = (ext2->blocksize / sizeof(uint32_t));


    // Direct Blocks
    if (block < a) {

        ext2_utils_write_block(ext2, blocks[block], offset, data, size);
        return;
    }


    // Indirect Blocks
    block -= a;

    if (block < b) {

        ext2_utils_read_block(ext2, blocks[EXT2_IND_BLOCK], sizeof(uint32_t) * block, &block, sizeof(uint32_t), true);
        ext2_utils_write_block(ext2, block, offset, data, size);
        return;
    }


    // Double Indirect Blocks
    block -= b;

    if (block < (b * b)) {

        register uint32_t p = block;

        ext2_utils_read_block(ext2, blocks[EXT2_DIND_BLOCK], sizeof(uint32_t) * (p / b), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * (p % b), &block, sizeof(uint32_t), true);
        ext2_utils_write_block(ext2, block, offset, data, size);

        return;
    }


    // Triply Indirect Blocks
    block -= b * b;

    if (block < (b * b * b)) {

        register uint32_t p = block;

        ext2_utils_read_block(ext2, blocks[EXT2_TIND_BLOCK], sizeof(uint32_t) * (p / (b * b)), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((p % (b * b)) / b), &block, sizeof(uint32_t), true);
        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((p % (b * b)) % b), &block, sizeof(uint32_t), true);
        ext2_utils_write_block(ext2, block, offset, data, size);
        return;
    }


    kpanicf("%s() FAIL! block is too high!", __func__);
}


void ext2_utils_alloc_inode_data(ext2_t* ext2, uint32_t* blocks, uint32_t block) {

    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(blocks);

    uint32_t a = EXT2_NDIR_BLOCKS, b = (ext2->blocksize / sizeof(uint32_t)), c = 0;


    // Direct Blocks
    if (block < a) {

        if (__block_is_free(blocks[block]))
            ext2_utils_alloc_block(ext2, &blocks[block]);

        return;
    }



    // Indirect Blocks
    block -= a;

    if (block < b) {

        if (__block_is_free(blocks[EXT2_IND_BLOCK]))
            ext2_utils_alloc_block(ext2, &blocks[EXT2_IND_BLOCK]);


        ext2_utils_read_block(ext2, blocks[EXT2_IND_BLOCK], sizeof(uint32_t) * block, &c, sizeof(uint32_t), true);

        if (__block_is_free(c)) {

            ext2_utils_alloc_block(ext2, &c);
            ext2_utils_write_block(ext2, blocks[EXT2_IND_BLOCK], sizeof(uint32_t) * block, &c, sizeof(uint32_t));
        }

        return;
    }



    // Double Indirect Blocks
    block -= b;

    if (block < (b * b)) {

        if (__block_is_free(blocks[EXT2_DIND_BLOCK]))
            ext2_utils_alloc_block(ext2, &blocks[EXT2_DIND_BLOCK]);


        ext2_utils_read_block(ext2, blocks[EXT2_DIND_BLOCK], sizeof(uint32_t) * (block / b), &c, sizeof(uint32_t), true);

        if (__block_is_free(c)) {

            ext2_utils_alloc_block(ext2, &c);
            ext2_utils_write_block(ext2, blocks[EXT2_DIND_BLOCK], sizeof(uint32_t) * (block / b), &c, sizeof(uint32_t));
        }

        block = c;


        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * (block % b), &c, sizeof(uint32_t), true);

        if (__block_is_free(c)) {

            ext2_utils_alloc_block(ext2, &c);
            ext2_utils_write_block(ext2, block, sizeof(uint32_t) * (block % b), &c, sizeof(uint32_t));
        }

        return;
    }



    // Triply Indirect Blocks
    block -= b * b;

    if (block < (b * b * b)) {

        if (__block_is_free(blocks[EXT2_TIND_BLOCK]))
            ext2_utils_alloc_block(ext2, &blocks[EXT2_TIND_BLOCK]);


        ext2_utils_read_block(ext2, blocks[EXT2_TIND_BLOCK], sizeof(uint32_t) * (block / (b * b)), &c, sizeof(uint32_t), true);

        if (__block_is_free(c)) {

            ext2_utils_alloc_block(ext2, &c);
            ext2_utils_write_block(ext2, blocks[EXT2_DIND_BLOCK], sizeof(uint32_t) * (block / (b * b)), &c, sizeof(uint32_t));
        }

        block = c;


        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) / b), &c, sizeof(uint32_t), true);

        if (__block_is_free(c)) {

            ext2_utils_alloc_block(ext2, &c);
            ext2_utils_write_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) / b), &c, sizeof(uint32_t));
        }

        block = c;


        ext2_utils_read_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) % b), &c, sizeof(uint32_t), true);

        if (__block_is_free(c)) {

            ext2_utils_alloc_block(ext2, &c);
            ext2_utils_write_block(ext2, block, sizeof(uint32_t) * ((block % (b * b)) % b), &c, sizeof(uint32_t));
        }

        return;
    }


    kpanicf("%s() FAIL! block is too high!", __func__);
}
