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
#include <sys/mount.h>

#include <aplus.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/vfs.h>

#include "ext2.h"


static int ext2_write_fresh(ext2_t* ext2, uint32_t physical, uint32_t inblock, const uint8_t* data, size_t size) {
    uint8_t edge[EXT2_MAX_BLOCK_SIZE];

    if (inblock > 0) {
        size_t chunk = ext2->blocksize - inblock;

        if (chunk > size)
            chunk = size;

        memset(edge, 0, ext2->blocksize);
        memcpy(edge + inblock, data, chunk);

        if (ext2_utils_write_block(ext2, physical, 0, edge, ext2->blocksize) < 0)
            return -1;

        physical++;
        data += chunk;
        size -= chunk;
    }

    size_t full = size - (size % ext2->blocksize);

    if (full > 0) {
        if (ext2_utils_write_blocks(ext2, physical, 0, data, full) < 0)
            return -1;

        physical += full / ext2->blocksize;
        data += full;
        size -= full;
    }

    if (size > 0) {
        memset(edge, 0, ext2->blocksize);
        memcpy(edge, data, size);

        if (ext2_utils_write_block(ext2, physical, 0, edge, ext2->blocksize) < 0)
            return -1;
    }

    return 0;
}


ssize_t ext2_write(inode_t* inode, const void* buf, off_t pos, size_t len) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(buf);

    if (unlikely(inode->sb->flags & MS_RDONLY))
        return errno = EROFS, -1;

    if (unlikely(pos < 0 || (uint64_t)pos > UINT64_MAX - len))
        return errno = EINVAL, -1;

    ext2_t* ext2            = inode->sb->fsinfo;
    struct ext2_inode* node = cache_get(&inode->sb->cache, inode->ino);
    uint64_t old_size       = ext2_inode_get_size(ext2, node);
    size_t done             = 0;
    int result              = 0;

    scoped_lock(&ext2->lock) {
        if ((uint64_t)pos > old_size && ext2_utils_zero_range(ext2, node, old_size, pos) < 0) {
            result = -1;
        }

        while (result == 0 && done < len) {
            uint64_t offset  = pos + done;
            uint32_t block   = offset / ext2->blocksize;
            uint32_t inblock = offset % ext2->blocksize;
            size_t blocks    = (inblock + (len - done) + ext2->blocksize - 1) / ext2->blocksize;
            uint32_t physical;
            bool fresh;

            long run = ext2_utils_map_alloc_blocks(ext2, node, block, blocks, &physical, &fresh);

            if (run < 0) {
                result = -1;
                break;
            }

            size_t chunk = (size_t)run * ext2->blocksize - inblock;

            if (chunk > len - done)
                chunk = len - done;

            const uint8_t* data = (const uint8_t*)buf + done;

            if ((fresh ? ext2_write_fresh(ext2, physical, inblock, data, chunk) : ext2_utils_write_blocks(ext2, physical, inblock, data, chunk)) < 0) {
                result = -1;
                break;
            }

            done += chunk;
        }

        if (ext2_utils_alloc_commit(ext2) < 0)
            result = -1;

        if (done > 0) {
            uint64_t new_size = pos + done;

            if (new_size > old_size)
                ext2_inode_set_size(ext2, node, new_size);

            node->i_mtime = arch_timer_gettime();
            node->i_ctime = node->i_mtime;
        }

        if (((uint64_t)pos > old_size || done > 0) && ext2_utils_write_inode(ext2, inode->ino, node) < 0)
            result = -1;
    }

    if (done > 0)
        return done;

    return result;
}
