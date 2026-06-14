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


int ext2_truncate(inode_t* inode, off_t len) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);

    if (unlikely(inode->sb->flags & MS_RDONLY))
        return errno = EROFS, -1;

    if (unlikely(len < 0))
        return errno = EINVAL, -1;

    ext2_t* ext2            = inode->sb->fsinfo;
    struct ext2_inode* node = cache_get(&inode->sb->cache, inode->ino);
    uint64_t old_size       = ext2_inode_get_size(ext2, node);
    uint64_t new_size       = len;
    int result              = 0;

    if (new_size == old_size)
        return 0;

    scoped_lock(&ext2->lock) {
        if (new_size > old_size) {
            uint8_t zero[EXT2_MAX_BLOCK_SIZE] = {0};
            uint64_t offset                   = old_size;

            while (offset < new_size) {
                uint32_t block   = offset / ext2->blocksize;
                uint32_t inblock = offset % ext2->blocksize;
                size_t chunk     = ext2->blocksize - inblock;

                if (chunk > new_size - offset)
                    chunk = new_size - offset;

                if (ext2_utils_write_inode_data(ext2, node, block, inblock, zero, chunk) < 0) {
                    result = -1;
                    break;
                }

                offset += chunk;
            }

            if (result == 0)
                ext2_inode_set_size(ext2, node, new_size);

        } else {
            uint32_t old_blocks = (old_size + ext2->blocksize - 1) / ext2->blocksize;
            uint32_t new_blocks = (new_size + ext2->blocksize - 1) / ext2->blocksize;

            if (new_size % ext2->blocksize) {
                uint8_t zero[EXT2_MAX_BLOCK_SIZE] = {0};
                uint32_t inblock                  = new_size % ext2->blocksize;

                if (ext2_utils_write_inode_data(ext2, node, new_size / ext2->blocksize, inblock, zero, ext2->blocksize - inblock) < 0)
                    result = -1;
            }

            for (uint32_t block = old_blocks; block > new_blocks; block--) {
                if (ext2_utils_free_inode_data(ext2, node, block - 1) < 0) {
                    result = -1;
                    break;
                }
            }

            ext2_inode_set_size(ext2, node, new_size);
        }

        node->i_mtime = arch_timer_gettime();
        node->i_ctime = node->i_mtime;

        if (ext2_utils_write_inode(ext2, inode->ino, node) < 0)
            result = -1;
    }

    return result;
}
