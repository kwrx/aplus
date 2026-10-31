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
#include <aplus/vfs.h>

#include "ext2.h"


int ext2_rename(inode_t* parent, const char* name, inode_t* into, const char* newname) {
    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(into);
    DEBUG_ASSERT(into->sb == parent->sb);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(newname);

    if (unlikely(parent->sb->flags & MS_RDONLY))
        return errno = EROFS, -1;

    size_t name_len = strlen(newname);

    if (unlikely(name_len == 0 || name_len > EXT2_NAME_LEN))
        return errno = ENAMETOOLONG, -1;

    inode_t* existing = ext2_finddir(into, newname);

    if (existing)
        return kfree(existing), errno = EEXIST, -1;

    if (errno != ENOENT)
        return -1;

    inode_t* source = ext2_finddir(parent, name);

    if (!source)
        return -1;

    ext2_t* ext2            = parent->sb->fsinfo;
    struct ext2_inode* dir  = cache_get(&parent->sb->cache, parent->ino);
    struct ext2_inode* dest = cache_get(&parent->sb->cache, into->ino);
    struct ext2_inode* node = cache_get(&parent->sb->cache, source->ino);
    bool moves_dir          = S_ISDIR(node->i_mode) && into->ino != parent->ino;
    ino_t ino               = 0;
    uint8_t type            = EXT2_FT_UNKNOWN;
    int result              = -1;

    scoped_lock(&ext2->lock) {
        if (moves_dir && dest->i_links_count >= EXT2_LINK_MAX) {
            errno = EMLINK;
            break;
        }

        if (ext2_utils_add_dir_entry(ext2, dest, source->ino, newname, node->i_mode) < 0)
            break;

        if (ext2_utils_remove_dir_entry(ext2, dir, name, &ino, &type) < 0) {
            int e = errno;

            ext2_utils_remove_dir_entry(ext2, dest, newname, &ino, &type);
            ext2_utils_write_inode(ext2, into->ino, dest);

            errno = e;
            break;
        }

        if (unlikely(ino != source->ino)) {
            errno = EIO;
            break;
        }

        if (moves_dir) {
            if (ext2_utils_set_dir_parent(ext2, node, into->ino) < 0)
                break;

            dir->i_links_count--;
            dest->i_links_count++;
        }

        node->i_ctime = arch_timer_gettime();

        if (ext2_utils_write_inode(ext2, source->ino, node) < 0)
            break;

        dir->i_mtime  = node->i_ctime;
        dir->i_ctime  = node->i_ctime;
        dest->i_mtime = node->i_ctime;
        dest->i_ctime = node->i_ctime;

        if (ext2_utils_write_inode(ext2, parent->ino, dir) < 0)
            break;

        if (into->ino != parent->ino && ext2_utils_write_inode(ext2, into->ino, dest) < 0)
            break;

        result = 0;
    }

    kfree(source);
    return result;
}
