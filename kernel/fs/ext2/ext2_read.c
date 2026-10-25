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

#include "ext2.h"


ssize_t ext2_read(inode_t* inode, void* buf, off_t pos, size_t len) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(buf);

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;

    ext2_t* ext2            = inode->sb->fsinfo;
    struct ext2_inode* node = cache_get(&inode->sb->cache, inode->ino);
    uint64_t size           = ext2_inode_get_size(ext2, node);

    if ((uint64_t)pos >= size)
        return 0;

    if (len > size - pos)
        len = size - pos;

    size_t done = 0;

    scoped_lock(&ext2->lock) {
        while (done < len) {

            uint32_t physical;
            uint64_t offset  = pos + done;
            uint32_t block   = offset / ext2->blocksize;
            uint32_t inblock = offset % ext2->blocksize;
            
            size_t blocks = (inblock + (len - done) + ext2->blocksize - 1) / ext2->blocksize;

            long run = ext2_utils_map_blocks(ext2, node, block, blocks, &physical);
            if (run < 0)
                return done ? (ssize_t)done : -1;

            size_t chunk = (size_t)run * ext2->blocksize - inblock;

            if (chunk > len - done)
                chunk = len - done;

            if (physical == 0) {
                memset((uint8_t*)buf + done, 0, chunk);
            } else if (ext2_utils_read_blocks(ext2, physical, inblock, (uint8_t*)buf + done, chunk) < 0) {
                return done ? (ssize_t)done : -1;
            }

            done += chunk;
        }
    }

    return done;
}
