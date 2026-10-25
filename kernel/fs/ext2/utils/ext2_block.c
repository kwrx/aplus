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
#include <aplus/hal.h>
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

/**
 * @brief Drops a block from the pointer-block cache of the read path.
 * @param ext2 The filesystem.
 * @param block The block being written or freed.
 */
static void ext2_mapcache_forget(ext2_t* ext2, uint32_t block) {
    for (size_t i = 0; i < EXT2_IND_LEVELS; i++) {
        if (ext2->mapcache[i].block == block)
            ext2->mapcache[i].block = 0;
    }
}


int ext2_utils_read_block(ext2_t* ext2, uint32_t block, uint32_t offset, void* data, size_t size, bool cached) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);

    (void)cached;

    if (unlikely(block == 0 || block >= ext2->sb.s_blocks_count || offset > ext2->blocksize || size > ext2->blocksize - offset))
        return errno = EIO, -1;

    if (unlikely(vfs_read(ext2->dev, data, ((off_t)block * ext2->blocksize) + offset, size) != (ssize_t)size))
        return errno = EIO, -1;

    return 0;
}


/**
 * @brief Reads from a run of physically consecutive blocks in a single device request.
 * @param ext2 The filesystem.
 * @param block The first block of the run.
 * @param offset The byte offset into the first block.
 * @param data The destination.
 * @param size The number of bytes, which may span several blocks.
 * @return 0 on success, -1 with errno set.
 */
int ext2_utils_read_blocks(ext2_t* ext2, uint32_t block, uint32_t offset, void* data, size_t size) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);

    uint64_t start = (uint64_t)block * ext2->blocksize + offset;
    uint64_t end   = (uint64_t)ext2->sb.s_blocks_count * ext2->blocksize;

    if (unlikely(block == 0 || block >= ext2->sb.s_blocks_count || offset >= ext2->blocksize || size > end - start))
        return errno = EIO, -1;

    if (unlikely(vfs_read(ext2->dev, data, (off_t)start, size) != (ssize_t)size))
        return errno = EIO, -1;

    return 0;
}


int ext2_utils_write_block(ext2_t* ext2, uint32_t block, uint32_t offset, const void* data, size_t size) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);

    if (unlikely(block == 0 || block >= ext2->sb.s_blocks_count || offset > ext2->blocksize || size > ext2->blocksize - offset))
        return errno = EIO, -1;

    ext2_mapcache_forget(ext2, block);

    if (unlikely(vfs_write(ext2->dev, data, ((off_t)block * ext2->blocksize) + offset, size) != (ssize_t)size))
        return errno = EIO, -1;

    return 0;
}


int ext2_utils_zero_block(ext2_t* ext2, uint32_t block) {
    DEBUG_ASSERT(ext2);

    uint8_t zero[EXT2_MAX_BLOCK_SIZE] = {0};
    return ext2_utils_write_block(ext2, block, 0, zero, ext2->blocksize);
}


int ext2_utils_read_group_desc(ext2_t* ext2, uint32_t group, struct ext2_group_desc* desc) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(desc);

    if (unlikely(group >= ext2->count_block_group))
        return errno = EINVAL, -1;

    uint32_t byte  = group * sizeof(*desc);
    uint32_t block = ext2->first_block_group + (byte / ext2->blocksize);
    uint32_t off   = byte % ext2->blocksize;

    return ext2_utils_read_block(ext2, block, off, desc, sizeof(*desc), false);
}


int ext2_utils_write_group_desc(ext2_t* ext2, uint32_t group, const struct ext2_group_desc* desc) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(desc);

    if (unlikely(group >= ext2->count_block_group))
        return errno = EINVAL, -1;

    uint32_t byte  = group * sizeof(*desc);
    uint32_t block = ext2->first_block_group + (byte / ext2->blocksize);
    uint32_t off   = byte % ext2->blocksize;

    return ext2_utils_write_block(ext2, block, off, desc, sizeof(*desc));
}


int ext2_utils_write_super(ext2_t* ext2) {
    DEBUG_ASSERT(ext2);

    ext2->sb.s_wtime = arch_timer_gettime();

    if (unlikely(vfs_write(ext2->dev, &ext2->sb, 1024, sizeof(ext2->sb)) != (ssize_t)sizeof(ext2->sb)))
        return errno = EIO, -1;

    return 0;
}


int ext2_utils_alloc_block(ext2_t* ext2, uint32_t* block) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);

    *block = 0;

    if (unlikely(ext2->sb.s_free_blocks_count == 0))
        return errno = ENOSPC, -1;

    for (uint32_t group = 0; group < ext2->count_block_group; group++) {
        struct ext2_group_desc desc;

        if (ext2_utils_read_group_desc(ext2, group, &desc) < 0)
            return -1;

        if (desc.bg_free_blocks_count == 0)
            continue;

        if (ext2_utils_read_block(ext2, desc.bg_block_bitmap, 0, ext2->iocache, ext2->blocksize, false) < 0)
            return -1;

        uint64_t group_start = ext2->sb.s_first_data_block + ((uint64_t)group * ext2->sb.s_blocks_per_group);
        uint32_t bits        = ext2->sb.s_blocks_per_group;

        if (group_start + bits > ext2->sb.s_blocks_count)
            bits = ext2->sb.s_blocks_count - group_start;

        for (uint32_t bit = 0; bit < bits; bit++) {
            uint32_t candidate = group_start + bit;

            if (ext2_bitmap_test(ext2->iocache, bit))
                continue;

            if (ext2_utils_zero_block(ext2, candidate) < 0)
                return -1;

            ext2_bitmap_set(ext2->iocache, bit);

            if (ext2_utils_write_block(ext2, desc.bg_block_bitmap, 0, ext2->iocache, ext2->blocksize) < 0)
                return -1;

            desc.bg_free_blocks_count--;
            ext2->sb.s_free_blocks_count--;

            if (ext2_utils_write_group_desc(ext2, group, &desc) < 0 || ext2_utils_write_super(ext2) < 0)
                return -1;

            ext2->root->sb->st.f_bfree--;
            ext2->root->sb->st.f_bavail--;

            *block = candidate;
            return 0;
        }
    }

    return errno = ENOSPC, -1;
}


int ext2_utils_free_block(ext2_t* ext2, uint32_t block) {
    DEBUG_ASSERT(ext2);

    if (unlikely(block < ext2->sb.s_first_data_block || block >= ext2->sb.s_blocks_count))
        return errno = EINVAL, -1;

    ext2_mapcache_forget(ext2, block);

    uint32_t relative = block - ext2->sb.s_first_data_block;
    uint32_t group    = relative / ext2->sb.s_blocks_per_group;
    uint32_t bit      = relative % ext2->sb.s_blocks_per_group;
    struct ext2_group_desc desc;

    if (ext2_utils_read_group_desc(ext2, group, &desc) < 0)
        return -1;

    if (ext2_utils_read_block(ext2, desc.bg_block_bitmap, 0, ext2->iocache, ext2->blocksize, false) < 0)
        return -1;

    if (unlikely(!ext2_bitmap_test(ext2->iocache, bit)))
        return errno = EINVAL, -1;

    ext2_bitmap_clear(ext2->iocache, bit);

    if (ext2_utils_write_block(ext2, desc.bg_block_bitmap, 0, ext2->iocache, ext2->blocksize) < 0)
        return -1;

    desc.bg_free_blocks_count++;
    ext2->sb.s_free_blocks_count++;

    if (ext2_utils_write_group_desc(ext2, group, &desc) < 0 || ext2_utils_write_super(ext2) < 0)
        return -1;

    ext2->root->sb->st.f_bfree++;
    ext2->root->sb->st.f_bavail++;

    return 0;
}
