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


/**
 * @brief Adjusts the directory count of the block group an inode belongs to.
 * @param ext2 The filesystem.
 * @param ino The inode of a directory being created or removed.
 * @param delta +1 for a new directory, -1 for a removed one.
 * @return 0 on success, -1 with errno set.
 */
int ext2_utils_count_dir(ext2_t* ext2, ino_t ino, int delta) {
    DEBUG_ASSERT(ext2);

    if (unlikely(ino == 0 || ino > ext2->sb.s_inodes_count))
        return errno = EINVAL, -1;

    uint32_t group = (ino - 1) / ext2->sb.s_inodes_per_group;
    struct ext2_group_desc desc;

    if (ext2_utils_read_group_desc(ext2, group, &desc) < 0)
        return -1;

    desc.bg_used_dirs_count += delta;

    return ext2_utils_write_group_desc(ext2, group, &desc);
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


/**
 * @brief Returns a block of block pointers through the cache slot of its level of indirection.
 * @param ext2 The filesystem, locked.
 * @param depth The level of indirection.
 * @param block The block of pointers.
 * @return The pointers, or NULL with errno set.
 */
static uint32_t* ext2_inode_pointer_block(ext2_t* ext2, uint32_t depth, uint32_t block) {
    ext2_mapcache_t* cache = &ext2->mapcache[depth];

    if (cache->block == block)
        return cache->pointers;

    if (unlikely(!cache->pointers && !(cache->pointers = (uint32_t*)kmalloc(ext2->blocksize, GFP_KERNEL))))
        return errno = ENOMEM, NULL;

    cache->block = 0;

    if (ext2_utils_read_block(ext2, block, 0, cache->pointers, ext2->blocksize, false) < 0)
        return NULL;

    cache->block = block;
    return cache->pointers;
}


/**
 * @brief Finds the physical block behind a logical block without allocating.
 * @param ext2 The filesystem, locked.
 * @param inode The inode.
 * @param logical The logical block.
 * @param physical Receives the physical block, or 0 for a hole.
 * @return 0 on success, -1 with errno set.
 */
static int ext2_inode_lookup(ext2_t* ext2, struct ext2_inode* inode, uint32_t logical, uint32_t* physical) {
    uint32_t indexes[EXT2_IND_LEVELS] = {0};
    uint32_t levels;
    uint32_t slot;

    if (ext2_inode_block_path(ext2, logical, &slot, indexes, &levels) < 0)
        return -1;

    uint32_t current = inode->i_block[slot];

    for (uint32_t depth = 0; depth < levels && current != 0; depth++) {
        uint32_t* pointers = ext2_inode_pointer_block(ext2, depth, current);

        if (unlikely(!pointers))
            return -1;

        current = pointers[indexes[depth]];
    }

    if (unlikely(current >= ext2->sb.s_blocks_count))
        return errno = EIO, -1;

    *physical = current;
    return 0;
}


/**
 * @brief Maps logical blocks onto a run of physically consecutive blocks, or onto a run of holes.
 * @param ext2 The filesystem, locked.
 * @param inode The inode.
 * @param logical The first logical block.
 * @param count The most blocks the run may cover.
 * @param physical Receives the first physical block of the run, or 0 for holes.
 * @return The number of blocks in the run, or -1 with errno set.
 */
long ext2_utils_map_blocks(ext2_t* ext2, struct ext2_inode* inode, uint32_t logical, size_t count, uint32_t* physical) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(physical);
    DEBUG_ASSERT(count > 0);

    uint32_t first;

    if (ext2_inode_lookup(ext2, inode, logical, &first) < 0)
        return -1;

    long run = 1;

    for (; (size_t)run < count; run++) {
        uint32_t next;

        if (ext2_inode_lookup(ext2, inode, logical + run, &next) < 0)
            break;

        if (first == 0 ? next != 0 : next != first + run)
            break;
    }

    *physical = first;
    return run;
}


/**
 * @brief Writes back a cached block of pointers and keeps it cached.
 * @param ext2 The filesystem, locked.
 * @param depth The level of indirection whose cache slot holds the block.
 * @param block The block of pointers.
 * @return 0 on success, -1 with errno set.
 */
static int ext2_inode_pointer_store(ext2_t* ext2, uint32_t depth, uint32_t block) {
    ext2_mapcache_t* cache = &ext2->mapcache[depth];

    DEBUG_ASSERT(cache->block == block);

    if (ext2_utils_write_block(ext2, block, 0, cache->pointers, ext2->blocksize) < 0)
        return -1;

    cache->block = block;
    return 0;
}


/**
 * @brief Allocates a zeroed block of pointers and caches it in the slot of its level of indirection.
 * @param ext2 The filesystem, locked.
 * @param depth The level of indirection.
 * @param block Receives the new block.
 * @return 0 on success, -1 with errno set.
 */
static int ext2_inode_pointer_alloc(ext2_t* ext2, uint32_t depth, uint32_t* block) {
    ext2_mapcache_t* cache = &ext2->mapcache[depth];

    if (unlikely(!cache->pointers && !(cache->pointers = (uint32_t*)kmalloc(ext2->blocksize, GFP_KERNEL))))
        return errno = ENOMEM, -1;

    if (ext2_utils_alloc_blocks(ext2, 0, 1, block) < 0)
        return -1;

    memset(cache->pointers, 0, ext2->blocksize);
    cache->block = *block;

    return ext2_inode_pointer_store(ext2, depth, *block);
}


/**
 * @brief Returns the last-level block of pointers for a logical block, allocating missing blocks of pointers on the way.
 * @param ext2 The filesystem, locked.
 * @param inode The inode.
 * @param slot The slot in i_block the path starts from.
 * @param indexes The index at each level of indirection.
 * @param levels The number of levels of indirection.
 * @param leaf Receives the last-level block.
 * @return Its pointers, or NULL with errno set.
 */
static uint32_t* ext2_inode_pointer_path(ext2_t* ext2, struct ext2_inode* inode, uint32_t slot, const uint32_t* indexes, uint32_t levels, uint32_t* leaf) {
    uint32_t current = inode->i_block[slot];

    if (current == 0) {
        if (ext2_inode_pointer_alloc(ext2, 0, &current) < 0)
            return NULL;

        inode->i_block[slot] = current;
        inode->i_blocks += ext2_sectors_per_block(ext2);
    }

    for (uint32_t depth = 0;; depth++) {
        uint32_t* pointers = ext2_inode_pointer_block(ext2, depth, current);

        if (unlikely(!pointers))
            return NULL;

        if (depth == levels - 1) {
            *leaf = current;
            return pointers;
        }

        uint32_t child = pointers[indexes[depth]];

        if (child == 0) {
            if (ext2_inode_pointer_alloc(ext2, depth + 1, &child) < 0)
                return NULL;

            inode->i_blocks += ext2_sectors_per_block(ext2);
            pointers[indexes[depth]] = child;

            if (ext2_inode_pointer_store(ext2, depth, current) < 0)
                return NULL;
        }

        current = child;
    }
}


/**
 * @brief Points consecutive logical blocks at consecutive physical blocks, one write per block of pointers.
 * @param ext2 The filesystem, locked.
 * @param inode The inode.
 * @param logical The first logical block.
 * @param first The first physical block.
 * @param count The number of blocks.
 * @return The number of blocks mapped, or -1 with errno set if none was.
 */
static long ext2_inode_map_run(ext2_t* ext2, struct ext2_inode* inode, uint32_t logical, uint32_t first, long count) {
    uint32_t per_block = ext2->blocksize / sizeof(uint32_t);
    long done          = 0;

    while (done < count) {
        uint32_t indexes[EXT2_IND_LEVELS] = {0};
        uint32_t levels;
        uint32_t slot;
        uint32_t leaf;

        if (ext2_inode_block_path(ext2, logical + done, &slot, indexes, &levels) < 0)
            break;

        if (levels == 0) {
            inode->i_block[slot] = first + done;
            inode->i_blocks += ext2_sectors_per_block(ext2);
            done++;
            continue;
        }

        uint32_t* pointers = ext2_inode_pointer_path(ext2, inode, slot, indexes, levels, &leaf);

        if (unlikely(!pointers))
            break;

        uint32_t index = indexes[levels - 1];
        long n         = count - done;

        if (n > (long)(per_block - index))
            n = per_block - index;

        for (long i = 0; i < n; i++)
            pointers[index + i] = first + done + i;

        if (ext2_inode_pointer_store(ext2, levels - 1, leaf) < 0)
            break;

        inode->i_blocks += n * ext2_sectors_per_block(ext2);
        done += n;
    }

    return done > 0 ? done : -1;
}


/**
 * @brief Maps logical blocks onto a run of physical blocks, allocating a contiguous run over holes without zeroing it.
 * @param ext2 The filesystem, locked; allocations stay pending until ext2_utils_alloc_commit.
 * @param inode The inode.
 * @param logical The first logical block.
 * @param count The most blocks the run may cover.
 * @param physical Receives the first physical block of the run.
 * @param fresh Receives true when the run was allocated by this call and holds garbage.
 * @return The number of blocks in the run, or -1 with errno set.
 */
long ext2_utils_map_alloc_blocks(ext2_t* ext2, struct ext2_inode* inode, uint32_t logical, size_t count, uint32_t* physical, bool* fresh) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(physical);
    DEBUG_ASSERT(fresh);

    long run = ext2_utils_map_blocks(ext2, inode, logical, count, physical);

    if (run < 0)
        return -1;

    *fresh = *physical == 0;

    if (!*fresh)
        return run;

    uint32_t goal = 0;
    uint32_t previous;
    uint32_t first;

    if (logical > 0 && ext2_inode_lookup(ext2, inode, logical - 1, &previous) == 0 && previous != 0)
        goal = previous + 1;

    long allocated = ext2_utils_alloc_blocks(ext2, goal, run, &first);

    if (allocated < 0)
        return -1;

    long mapped = ext2_inode_map_run(ext2, inode, logical, first, allocated);

    for (long i = mapped < 0 ? 0 : mapped; i < allocated; i++)
        ext2_utils_free_block(ext2, first + i);

    if (mapped < 0)
        return -1;

    *physical = first;
    return mapped;
}


/**
 * @brief Zeroes a byte range inside the block that holds its start, if that block is allocated.
 * @param ext2 The filesystem, locked.
 * @param inode The inode.
 * @param start The first byte; nothing happens when it is block-aligned.
 * @param end The byte after the range, clipped to the end of the block.
 * @return 0 on success, -1 with errno set.
 */
int ext2_utils_zero_range(ext2_t* ext2, struct ext2_inode* inode, uint64_t start, uint64_t end) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);

    uint32_t inblock = start % ext2->blocksize;
    uint32_t physical;

    if (inblock == 0 || end <= start)
        return 0;

    if (end - start > ext2->blocksize - inblock)
        end = start + ext2->blocksize - inblock;

    if (ext2_inode_lookup(ext2, inode, start / ext2->blocksize, &physical) < 0)
        return -1;

    if (physical == 0)
        return 0;

    uint8_t zero[EXT2_MAX_BLOCK_SIZE] = {0};
    return ext2_utils_write_block(ext2, physical, inblock, zero, end - start);
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


/**
 * @brief Frees what a block of pointers maps from a logical offset onward, and the block itself once it maps nothing.
 * @param ext2 The filesystem, locked; frees stay pending until ext2_utils_alloc_commit.
 * @param inode The inode.
 * @param depth The level of indirection of the block.
 * @param levels The levels of indirection below and including the block.
 * @param block The block of pointers.
 * @param from The first logical block to free, relative to the start of the block's range.
 * @return 1 if the block was freed, 0 if it still maps blocks, -1 with errno set.
 */
static int ext2_inode_free_tree(ext2_t* ext2, struct ext2_inode* inode, uint32_t depth, uint32_t levels, uint32_t block, uint64_t from) {
    uint32_t per_block = ext2->blocksize / sizeof(uint32_t);
    uint64_t span      = 1;

    for (uint32_t i = 1; i < levels; i++)
        span *= per_block;

    uint32_t* pointers = ext2_inode_pointer_block(ext2, depth, block);

    if (unlikely(!pointers))
        return -1;

    bool changed = false;

    for (uint32_t i = from / span; i < per_block; i++) {
        uint32_t child = pointers[i];

        if (child == 0)
            continue;

        if (levels == 1) {
            if (ext2_utils_release_block(ext2, child) < 0)
                return -1;

            inode->i_blocks -= ext2_sectors_per_block(ext2);

        } else {
            uint64_t start = (uint64_t)i * span;
            int freed      = ext2_inode_free_tree(ext2, inode, depth + 1, levels - 1, child, from > start ? from - start : 0);

            if (freed < 0)
                return -1;

            if (freed == 0)
                continue;
        }

        pointers[i] = 0;
        changed     = true;
    }

    for (uint32_t i = 0; i < per_block; i++) {
        if (pointers[i] != 0)
            return changed && ext2_inode_pointer_store(ext2, depth, block) < 0 ? -1 : 0;
    }

    if (ext2_utils_release_block(ext2, block) < 0)
        return -1;

    inode->i_blocks -= ext2_sectors_per_block(ext2);
    return 1;
}


/**
 * @brief Frees every block of an inode from a logical block onward, committing the bitmaps, descriptors and superblock once.
 * @param ext2 The filesystem, locked.
 * @param inode The inode.
 * @param from The first logical block to free.
 * @return 0 on success, -1 with errno set.
 */
int ext2_utils_free_inode_range(ext2_t* ext2, struct ext2_inode* inode, uint32_t from) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(inode);

    uint64_t per_block = ext2->blocksize / sizeof(uint32_t);
    uint64_t base      = EXT2_NDIR_BLOCKS;
    uint64_t span      = per_block;
    int result         = 0;

    for (uint32_t slot = from; slot < EXT2_NDIR_BLOCKS && result == 0; slot++) {
        if (inode->i_block[slot] == 0)
            continue;

        if (ext2_utils_release_block(ext2, inode->i_block[slot]) < 0) {
            result = -1;
            break;
        }

        inode->i_block[slot] = 0;
        inode->i_blocks -= ext2_sectors_per_block(ext2);
    }

    for (uint32_t levels = 1; levels <= EXT2_IND_LEVELS && result == 0; levels++) {
        uint32_t slot = EXT2_NDIR_BLOCKS + levels - 1;

        if (inode->i_block[slot] != 0 && from < base + span) {
            int freed = ext2_inode_free_tree(ext2, inode, 0, levels, inode->i_block[slot], from > base ? from - base : 0);

            if (freed < 0)
                result = -1;
            else if (freed > 0)
                inode->i_block[slot] = 0;
        }

        base += span;
        span *= per_block;
    }

    if (ext2_utils_alloc_commit(ext2) < 0)
        result = -1;

    return result;
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
