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


static int ext2_zero_range(ext2_t* ext2, struct ext2_inode* inode, uint64_t start, uint64_t end) {
    uint8_t zero[EXT2_MAX_BLOCK_SIZE] = {0};

    while (start < end) {
        uint32_t block   = start / ext2->blocksize;
        uint32_t inblock = start % ext2->blocksize;
        size_t chunk     = ext2->blocksize - inblock;

        if (chunk > end - start)
            chunk = end - start;

        if (ext2_utils_write_inode_data(ext2, inode, block, inblock, zero, chunk) < 0)
            return -1;

        start += chunk;
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
        if ((uint64_t)pos > old_size && ext2_zero_range(ext2, node, old_size, pos) < 0) {
            result = -1;
        }

        while (result == 0 && done < len) {
            uint64_t offset  = pos + done;
            uint32_t block   = offset / ext2->blocksize;
            uint32_t inblock = offset % ext2->blocksize;
            size_t chunk     = ext2->blocksize - inblock;

            if (chunk > len - done)
                chunk = len - done;

            if (ext2_utils_write_inode_data(ext2, node, block, inblock, (const uint8_t*)buf + done, chunk) < 0) {
                result = -1;
                break;
            }

            done += chunk;
        }

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
