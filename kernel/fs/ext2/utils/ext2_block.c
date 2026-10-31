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
#include <aplus/memory.h>
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


int ext2_utils_write_blocks(ext2_t* ext2, uint32_t block, uint32_t offset, const void* data, size_t size) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(data);

    uint64_t start = (uint64_t)block * ext2->blocksize + offset;
    uint64_t end   = (uint64_t)ext2->sb.s_blocks_count * ext2->blocksize;

    if (unlikely(block == 0 || block >= ext2->sb.s_blocks_count || offset >= ext2->blocksize || size > end - start))
        return errno = EIO, -1;

    for (size_t i = 0; i < EXT2_IND_LEVELS; i++) {
        uint64_t cached = (uint64_t)ext2->mapcache[i].block * ext2->blocksize;

        if (cached + ext2->blocksize > start && cached < start + size)
            ext2->mapcache[i].block = 0;
    }

    if (unlikely(vfs_write(ext2->dev, data, (off_t)start, size) != (ssize_t)size))
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


static int ext2_balloc_flush(ext2_t* ext2) {
    ext2_balloc_t* balloc = &ext2->balloc;

    if (!balloc->dirty)
        return 0;

    if (ext2_utils_write_block(ext2, balloc->desc.bg_block_bitmap, 0, balloc->bitmap, ext2->blocksize) < 0)
        return -1;

    if (ext2_utils_write_group_desc(ext2, balloc->group, &balloc->desc) < 0)
        return -1;

    balloc->dirty = false;
    return 0;
}


static int ext2_balloc_load(ext2_t* ext2, uint32_t group, const struct ext2_group_desc* desc) {
    ext2_balloc_t* balloc = &ext2->balloc;

    if (balloc->loaded && balloc->group == group)
        return 0;

    if (ext2_balloc_flush(ext2) < 0)
        return -1;

    balloc->loaded = false;

    if (unlikely(!balloc->bitmap && !(balloc->bitmap = (uint8_t*)kmalloc(ext2->blocksize, GFP_KERNEL))))
        return errno = ENOMEM, -1;

    if (desc)
        balloc->desc = *desc;
    else if (ext2_utils_read_group_desc(ext2, group, &balloc->desc) < 0)
        return -1;

    if (ext2_utils_read_block(ext2, balloc->desc.bg_block_bitmap, 0, balloc->bitmap, ext2->blocksize, false) < 0)
        return -1;

    balloc->group  = group;
    balloc->loaded = true;
    return 0;
}


int ext2_utils_alloc_commit(ext2_t* ext2) {
    DEBUG_ASSERT(ext2);

    int result = ext2_balloc_flush(ext2);

    ext2->balloc.loaded = false;

    if (ext2->sb_dirty) {
        ext2->sb_dirty = false;

        if (ext2_utils_write_super(ext2) < 0)
            result = -1;
    }

    return result;
}


long ext2_utils_alloc_blocks(ext2_t* ext2, uint32_t goal, size_t count, uint32_t* first) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(first);
    DEBUG_ASSERT(count > 0);

    *first = 0;

    if (unlikely(ext2->sb.s_free_blocks_count == 0))
        return errno = ENOSPC, -1;

    if (goal == 0)
        goal = ext2->goal;

    if (goal < ext2->sb.s_first_data_block || goal >= ext2->sb.s_blocks_count)
        goal = ext2->sb.s_first_data_block;

    uint32_t start_group = (goal - ext2->sb.s_first_data_block) / ext2->sb.s_blocks_per_group;
    uint32_t start_bit   = (goal - ext2->sb.s_first_data_block) % ext2->sb.s_blocks_per_group;

    for (uint32_t i = 0; i <= ext2->count_block_group; i++) {
        uint32_t group = (start_group + i) % ext2->count_block_group;

        if (!ext2->balloc.loaded || ext2->balloc.group != group) {
            struct ext2_group_desc desc;

            if (ext2_utils_read_group_desc(ext2, group, &desc) < 0)
                return -1;

            if (desc.bg_free_blocks_count == 0)
                continue;

            if (ext2_balloc_load(ext2, group, &desc) < 0)
                return -1;

        } else if (ext2->balloc.desc.bg_free_blocks_count == 0) {
            continue;
        }

        uint8_t* bitmap      = ext2->balloc.bitmap;
        uint64_t group_start = ext2->sb.s_first_data_block + ((uint64_t)group * ext2->sb.s_blocks_per_group);
        uint32_t bits        = ext2->sb.s_blocks_per_group;
        uint32_t bit         = i == 0 ? start_bit : 0;

        if (group_start + bits > ext2->sb.s_blocks_count)
            bits = ext2->sb.s_blocks_count - group_start;

        while (bit < bits) {
            if ((bit & 7) == 0 && bitmap[bit >> 3] == 0xFF)
                bit += 8;
            else if (ext2_bitmap_test(bitmap, bit))
                bit++;
            else
                break;
        }

        if (bit >= bits)
            continue;

        uint32_t run = 0;

        while (run < count && run < ext2->balloc.desc.bg_free_blocks_count && bit + run < bits && !ext2_bitmap_test(bitmap, bit + run)) {
            ext2_bitmap_set(bitmap, bit + run);
            run++;
        }

        ext2->balloc.desc.bg_free_blocks_count -= run;
        ext2->balloc.dirty = true;

        ext2->sb.s_free_blocks_count -= run;
        ext2->sb_dirty = true;

        ext2->root->sb->st.f_bfree -= run;
        ext2->root->sb->st.f_bavail -= run;

        *first     = group_start + bit;
        ext2->goal = *first + run;

        return run;
    }

    return errno = ENOSPC, -1;
}


int ext2_utils_alloc_block(ext2_t* ext2, uint32_t* block) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(block);

    int result = 0;

    if (ext2_utils_alloc_blocks(ext2, 0, 1, block) < 0 || ext2_utils_zero_block(ext2, *block) < 0)
        result = -1;

    if (ext2_utils_alloc_commit(ext2) < 0)
        result = -1;

    return result;
}


int ext2_utils_release_block(ext2_t* ext2, uint32_t block) {
    DEBUG_ASSERT(ext2);

    if (unlikely(block < ext2->sb.s_first_data_block || block >= ext2->sb.s_blocks_count))
        return errno = EINVAL, -1;

    ext2_mapcache_forget(ext2, block);

    uint32_t relative = block - ext2->sb.s_first_data_block;
    uint32_t group    = relative / ext2->sb.s_blocks_per_group;
    uint32_t bit      = relative % ext2->sb.s_blocks_per_group;

    if (ext2_balloc_load(ext2, group, NULL) < 0)
        return -1;

    if (unlikely(!ext2_bitmap_test(ext2->balloc.bitmap, bit)))
        return errno = EINVAL, -1;

    ext2_bitmap_clear(ext2->balloc.bitmap, bit);

    ext2->balloc.desc.bg_free_blocks_count++;
    ext2->balloc.dirty = true;

    ext2->sb.s_free_blocks_count++;
    ext2->sb_dirty = true;

    ext2->root->sb->st.f_bfree++;
    ext2->root->sb->st.f_bavail++;

    return 0;
}


int ext2_utils_free_block(ext2_t* ext2, uint32_t block) {
    DEBUG_ASSERT(ext2);

    int result = ext2_utils_release_block(ext2, block);

    if (ext2_utils_alloc_commit(ext2) < 0)
        result = -1;

    return result;
}
