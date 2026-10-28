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
            if (ext2_utils_zero_range(ext2, node, old_size, new_size) < 0)
                result = -1;

            if (result == 0)
                ext2_inode_set_size(ext2, node, new_size);

        } else {
            uint32_t old_blocks = (old_size + ext2->blocksize - 1) / ext2->blocksize;
            uint32_t new_blocks = (new_size + ext2->blocksize - 1) / ext2->blocksize;

            if (ext2_utils_zero_range(ext2, node, new_size, old_size) < 0)
                result = -1;

            if (new_blocks < old_blocks && ext2_utils_free_inode_range(ext2, node, new_blocks) < 0)
                result = -1;

            ext2_inode_set_size(ext2, node, new_size);
        }

        node->i_mtime = arch_timer_gettime();
        node->i_ctime = node->i_mtime;

        if (ext2_utils_write_inode(ext2, inode->ino, node) < 0)
            result = -1;
    }

    return result;
}
