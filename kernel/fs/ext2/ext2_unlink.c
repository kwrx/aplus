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


/**
 * @brief Removes a regular file or an empty directory from a directory.
 * @param parent The directory.
 * @param name The name of the entry.
 * @return 0 on success, -1 with errno set.
 */
int ext2_unlink(inode_t* parent, const char* name) {
    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(name);

    if (unlikely(parent->sb->flags & MS_RDONLY))
        return errno = EROFS, -1;

    inode_t* target = ext2_finddir(parent, name);

    if (!target)
        return -1;

    struct ext2_inode* node = cache_get(&parent->sb->cache, target->ino);

    bool is_dir = S_ISDIR(node->i_mode);

    if (!S_ISREG(node->i_mode) && !is_dir)
        return kfree(target), errno = EPERM, -1;

    ext2_t* ext2           = parent->sb->fsinfo;
    struct ext2_inode* dir = cache_get(&parent->sb->cache, parent->ino);
    ino_t ino              = 0;
    uint8_t type           = EXT2_FT_UNKNOWN;
    int result             = -1;

    scoped_lock(&ext2->lock) {
        if (is_dir) {
            int empty = ext2_utils_dir_empty(ext2, node);

            if (empty <= 0) {
                errno = empty < 0 ? errno : ENOTEMPTY;
                break;
            }
        }

        if (ext2_utils_remove_dir_entry(ext2, dir, name, &ino, &type) < 0)
            break;

        if (unlikely(ino != target->ino)) {
            errno = EIO;
            break;
        }

        if (!is_dir && node->i_links_count > 1) {
            node->i_links_count--;
            node->i_ctime = arch_timer_gettime();

            if (ext2_utils_write_inode(ext2, ino, node) < 0)
                break;

        } else {
            uint64_t size   = ext2_inode_get_size(ext2, node);
            uint32_t blocks = (size + ext2->blocksize - 1) / ext2->blocksize;
            bool failed     = false;

            for (uint32_t block = blocks; block > 0; block--) {
                if (ext2_utils_free_inode_data(ext2, node, block - 1) < 0) {
                    failed = true;
                    break;
                }
            }

            if (failed)
                break;

            memset(node, 0, ext2->inodesize);

            if (ext2_utils_write_inode(ext2, ino, node) < 0 || ext2_utils_free_inode(ext2, ino) < 0)
                break;
        }

        if (is_dir) {
            if (ext2_utils_count_dir(ext2, ino, -1) < 0)
                break;

            dir->i_links_count--;
        }

        dir->i_mtime = arch_timer_gettime();
        dir->i_ctime = dir->i_mtime;

        if (ext2_utils_write_inode(ext2, parent->ino, dir) < 0)
            break;

        if (node->i_links_count == 0)
            cache_remove(&parent->sb->cache, ino);

        result = 0;
    }

    kfree(target);
    return result;
}
