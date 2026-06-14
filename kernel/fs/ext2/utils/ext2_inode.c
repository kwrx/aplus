/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 * This file is part of aplus.
 */

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/errno.h>
#include <aplus/vfs.h>

#include "../ext2.h"


static inline bool ext2_bitmap_test(const uint8_t* bitmap, uint32_t bit) {
    return bitmap[bit >> 3] & (1U << (bit & 7));
}

static inline void ext2_bitmap_set(uint8_t* bitmap, uint32_t bit) {
    bitmap[bit >> 3] |= 1U << (bit & 7);
}

static inline void ext2_bitmap_clear(uint8_t* bitmap, uint32_t bit) {
    bitmap[bit >> 3] &= ~(1U << (bit & 7));
}

static inline uint32_t ext2_sectors_per_block(ext2_t* ext2) {
    return ext2->blocksize / 512;
}


static int ext2_inode_location(ext2_t* ext2, ino_t ino, uint32_t* block, uint32_t* offset) {
    if (unlikely(ino == 0 || ino > ext2->sb.s_inodes_count))
        return errno = EINVAL, -1;

    uint32_t group = (ino - 1) / ext2->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % ext2->sb.s_inodes_per_group;
    uint32_t byte  = index * ext2->inodesize;
    struct ext2_group_desc desc;

    if (ext2_utils_read_group_desc(ext2, group, &desc) < 0)
        return -1;

    *block  = desc.bg_inode_table + (byte / ext2->blocksize);
    *offset = byte % ext2->blocksize;

    if (unlikely(*offset + ext2->inodesize > ext2->blocksize))
        return errno = EIO, -1;

    return 0;
}


int ext2_utils_read_inode(ext2_t* ext2, ino_t ino, void* data) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);

    uint32_t block;
    uint32_t offset;

    if (ext2_inode_location(ext2, ino, &block, &offset) < 0)
        return -1;

    return ext2_utils_read_block(ext2, block, offset, data, ext2->inodesize, false);
}


int ext2_utils_write_inode(ext2_t* ext2, ino_t ino, const void* data) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);

    uint32_t block;
    uint32_t offset;

    if (ext2_inode_location(ext2, ino, &block, &offset) < 0)
        return -1;

    return ext2_utils_write_block(ext2, block, offset, data, ext2->inodesize);
}


int ext2_utils_alloc_inode(ext2_t* ext2, const struct ext2_inode* inode, ino_t* ino) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(ino);

    *ino = 0;

    if (unlikely(ext2->sb.s_free_inodes_count == 0))
        return errno = ENOSPC, -1;

    for (uint32_t group = 0; group < ext2->count_block_group; group++) {
        struct ext2_group_desc desc;

        if (ext2_utils_read_group_desc(ext2, group, &desc) < 0)
            return -1;

        if (desc.bg_free_inodes_count == 0)
            continue;

        if (ext2_utils_read_block(ext2, desc.bg_inode_bitmap, 0, ext2->iocache, ext2->blocksize, false) < 0)
            return -1;

        uint64_t group_start = (uint64_t)group * ext2->sb.s_inodes_per_group;
        uint32_t bits        = ext2->sb.s_inodes_per_group;

        if (group_start + bits > ext2->sb.s_inodes_count)
            bits = ext2->sb.s_inodes_count - group_start;

        for (uint32_t bit = 0; bit < bits; bit++) {
            ino_t candidate = group_start + bit + 1;

            if (candidate < ext2->sb.s_first_ino || ext2_bitmap_test(ext2->iocache, bit))
                continue;

            ext2_bitmap_set(ext2->iocache, bit);

            if (ext2_utils_write_block(ext2, desc.bg_inode_bitmap, 0, ext2->iocache, ext2->blocksize) < 0)
                return -1;

            desc.bg_free_inodes_count--;
            ext2->sb.s_free_inodes_count--;

            if (ext2_utils_write_group_desc(ext2, group, &desc) < 0 || ext2_utils_write_super(ext2) < 0 || ext2_utils_write_inode(ext2, candidate, inode) < 0)
                return -1;

            ext2->root->sb->st.f_ffree--;
            ext2->root->sb->st.f_favail--;

            *ino = candidate;
            return 0;
        }
    }

    return errno = ENOSPC, -1;
}


int ext2_utils_free_inode(ext2_t* ext2, ino_t ino) {
    DEBUG_ASSERT(ext2);

    if (unlikely(ino < ext2->sb.s_first_ino || ino > ext2->sb.s_inodes_count))
        return errno = EINVAL, -1;

    uint32_t group = (ino - 1) / ext2->sb.s_inodes_per_group;
    uint32_t bit   = (ino - 1) % ext2->sb.s_inodes_per_group;
    struct ext2_group_desc desc;

    if (ext2_utils_read_group_desc(ext2, group, &desc) < 0)
        return -1;

    if (ext2_utils_read_block(ext2, desc.bg_inode_bitmap, 0, ext2->iocache, ext2->blocksize, false) < 0)
        return -1;

    if (unlikely(!ext2_bitmap_test(ext2->iocache, bit)))
        return errno = EINVAL, -1;

    ext2_bitmap_clear(ext2->iocache, bit);

    if (ext2_utils_write_block(ext2, desc.bg_inode_bitmap, 0, ext2->iocache, ext2->blocksize) < 0)
        return -1;

    desc.bg_free_inodes_count++;
    ext2->sb.s_free_inodes_count++;

    if (ext2_utils_write_group_desc(ext2, group, &desc) < 0 || ext2_utils_write_super(ext2) < 0)
        return -1;

    ext2->root->sb->st.f_ffree++;
    ext2->root->sb->st.f_favail++;

    return 0;
}


static int ext2_inode_block_path(ext2_t* ext2, uint32_t logical, uint32_t* slot, uint32_t indexes[3], uint32_t* levels) {
    uint64_t per_block = ext2->blocksize / sizeof(uint32_t);

    if (logical < EXT2_NDIR_BLOCKS) {
        *slot   = logical;
        *levels = 0;
        return 0;
    }

    logical -= EXT2_NDIR_BLOCKS;

    if (logical < per_block) {
        *slot      = EXT2_IND_BLOCK;
        indexes[0] = logical;
        *levels    = 1;
        return 0;
    }

    logical -= per_block;

    if (logical < per_block * per_block) {
        *slot      = EXT2_DIND_BLOCK;
        indexes[0] = logical / per_block;
        indexes[1] = logical % per_block;
        *levels    = 2;
        return 0;
    }

    logical -= per_block * per_block;

    if ((uint64_t)logical < per_block * per_block * per_block) {
        *slot      = EXT2_TIND_BLOCK;
        indexes[0] = logical / (per_block * per_block);
        indexes[1] = (logical / per_block) % per_block;
        indexes[2] = logical % per_block;
        *levels    = 3;
        return 0;
    }

    return errno = EFBIG, -1;
}


static int ext2_inode_get_block(ext2_t* ext2, struct ext2_inode* inode, uint32_t logical, bool create, uint32_t* physical) {
    uint32_t indexes[3] = {0};
    uint32_t levels;
    uint32_t slot;

    if (ext2_inode_block_path(ext2, logical, &slot, indexes, &levels) < 0)
        return -1;

    if (levels == 0) {
        if (inode->i_block[slot] == 0 && create) {
            if (ext2_utils_alloc_block(ext2, &inode->i_block[slot]) < 0)
                return -1;

            inode->i_blocks += ext2_sectors_per_block(ext2);
        }

        *physical = inode->i_block[slot];
        return 0;
    }

    uint32_t current = inode->i_block[slot];

    if (current == 0) {
        if (!create) {
            *physical = 0;
            return 0;
        }

        if (ext2_utils_alloc_block(ext2, &current) < 0)
            return -1;

        inode->i_block[slot] = current;
        inode->i_blocks += ext2_sectors_per_block(ext2);
    }

    for (uint32_t depth = 0; depth < levels; depth++) {
        uint32_t child = 0;

        if (ext2_utils_read_block(ext2, current, indexes[depth] * sizeof(child), &child, sizeof(child), false) < 0)
            return -1;

        if (child == 0) {
            if (!create) {
                *physical = 0;
                return 0;
            }

            if (ext2_utils_alloc_block(ext2, &child) < 0)
                return -1;

            if (ext2_utils_write_block(ext2, current, indexes[depth] * sizeof(child), &child, sizeof(child)) < 0)
                return -1;

            inode->i_blocks += ext2_sectors_per_block(ext2);
        }

        current = child;
    }

    *physical = current;
    return 0;
}


static bool ext2_inode_pointer_block_empty(ext2_t* ext2, uint32_t block) {
    if (ext2_utils_read_block(ext2, block, 0, ext2->iocache, ext2->blocksize, false) < 0)
        return false;

    uint32_t* pointers = ext2->iocache;

    for (uint32_t i = 0; i < ext2->blocksize / sizeof(*pointers); i++) {
        if (pointers[i] != 0)
            return false;
    }

    return true;
}


int ext2_utils_free_inode_data(ext2_t* ext2, struct ext2_inode* inode, uint32_t logical) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);

    uint32_t indexes[3]  = {0};
    uint32_t pointers[3] = {0};
    uint32_t levels;
    uint32_t slot;

    if (ext2_inode_block_path(ext2, logical, &slot, indexes, &levels) < 0)
        return -1;

    if (levels == 0) {
        if (inode->i_block[slot] == 0)
            return 0;

        if (ext2_utils_free_block(ext2, inode->i_block[slot]) < 0)
            return -1;

        inode->i_block[slot] = 0;
        inode->i_blocks -= ext2_sectors_per_block(ext2);
        return 0;
    }

    uint32_t current = inode->i_block[slot];

    if (current == 0)
        return 0;

    for (uint32_t depth = 0; depth < levels; depth++) {
        pointers[depth] = current;

        if (ext2_utils_read_block(ext2, current, indexes[depth] * sizeof(current), &current, sizeof(current), false) < 0)
            return -1;

        if (current == 0)
            return 0;
    }

    if (ext2_utils_free_block(ext2, current) < 0)
        return -1;

    inode->i_blocks -= ext2_sectors_per_block(ext2);
    current = 0;

    if (ext2_utils_write_block(ext2, pointers[levels - 1], indexes[levels - 1] * sizeof(current), &current, sizeof(current)) < 0)
        return -1;

    for (int depth = levels - 1; depth >= 0; depth--) {
        if (!ext2_inode_pointer_block_empty(ext2, pointers[depth]))
            break;

        if (ext2_utils_free_block(ext2, pointers[depth]) < 0)
            return -1;

        inode->i_blocks -= ext2_sectors_per_block(ext2);

        if (depth == 0) {
            inode->i_block[slot] = 0;
        } else {
            if (ext2_utils_write_block(ext2, pointers[depth - 1], indexes[depth - 1] * sizeof(current), &current, sizeof(current)) < 0)
                return -1;
        }
    }

    return 0;
}


int ext2_utils_read_inode_data(ext2_t* ext2, struct ext2_inode* inode, uint32_t block, uint32_t offset, void* data, size_t size) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(data);

    if (unlikely(offset > ext2->blocksize || size > ext2->blocksize - offset))
        return errno = EINVAL, -1;

    uint32_t physical;

    if (ext2_inode_get_block(ext2, inode, block, false, &physical) < 0)
        return -1;

    if (physical == 0) {
        memset(data, 0, size);
        return 0;
    }

    return ext2_utils_read_block(ext2, physical, offset, data, size, false);
}


int ext2_utils_write_inode_data(ext2_t* ext2, struct ext2_inode* inode, uint32_t block, uint32_t offset, const void* data, size_t size) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(data);

    if (unlikely(offset > ext2->blocksize || size > ext2->blocksize - offset))
        return errno = EINVAL, -1;

    uint32_t physical;

    if (ext2_inode_get_block(ext2, inode, block, true, &physical) < 0)
        return -1;

    return ext2_utils_write_block(ext2, physical, offset, data, size);
}
