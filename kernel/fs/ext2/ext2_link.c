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
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include "ext2.h"



#include <stdint.h>
#include <string.h>
#include <sys/mount.h>

#include <aplus.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include "ext2.h"


/**
 * @brief Adds a new name in a directory for an existing regular file or symbolic link.
 * @param parent The directory.
 * @param target The inode the new name refers to.
 * @param name The new name.
 * @return 0 on success, -1 with errno set.
 */
int ext2_link(inode_t* parent, inode_t* target, const char* name) {
    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(target);
    DEBUG_ASSERT(name);

    if (unlikely(parent->sb->flags & MS_RDONLY))
        return errno = EROFS, -1;

    if (unlikely(target->sb != parent->sb))
        return errno = EXDEV, -1;

    size_t name_len = strlen(name);

    if (unlikely(name_len == 0 || name_len > EXT2_NAME_LEN))
        return errno = ENAMETOOLONG, -1;

    inode_t* existing = ext2_finddir(parent, name);

    if (existing)
        return kfree(existing), errno = EEXIST, -1;

    if (errno != ENOENT)
        return -1;

    ext2_t* ext2            = parent->sb->fsinfo;
    struct ext2_inode* dir  = cache_get(&parent->sb->cache, parent->ino);
    struct ext2_inode* node = cache_get(&parent->sb->cache, target->ino);
    int result              = -1;

    scoped_lock(&ext2->lock) {
        if (!S_ISREG(node->i_mode) && !S_ISLNK(node->i_mode)) {
            errno = EPERM;
            break;
        }

        if (node->i_links_count == 0) {
            errno = ENOENT;
            break;
        }

        if (node->i_links_count >= EXT2_LINK_MAX) {
            errno = EMLINK;
            break;
        }

        node->i_links_count++;
        node->i_ctime = arch_timer_gettime();

        if (ext2_utils_write_inode(ext2, target->ino, node) < 0) {
            node->i_links_count--;
            break;
        }

        if (ext2_utils_add_dir_entry(ext2, dir, target->ino, name, node->i_mode) < 0) {
            int e = errno;

            node->i_links_count--;
            ext2_utils_write_inode(ext2, target->ino, node);

            errno = e;
            break;
        }

        dir->i_mtime = arch_timer_gettime();
        dir->i_ctime = dir->i_mtime;

        if (ext2_utils_write_inode(ext2, parent->ino, dir) < 0)
            break;

        result = 0;
    }

    return result;
}
