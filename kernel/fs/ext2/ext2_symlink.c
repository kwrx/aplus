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


static void ext2_symlink_rollback(ext2_t* ext2, struct ext2_inode* node, ino_t ino) {
    int e = errno;

    if (!ext2_inode_is_fast_symlink(node))
        ext2_utils_free_inode_data(ext2, node, 0);

    memset(node, 0, ext2->inodesize);

    ext2_utils_write_inode(ext2, ino, node);
    ext2_utils_free_inode(ext2, ino);

    errno = e;
}


int ext2_symlink(inode_t* parent, const char* name, const char* target) {
    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(target);

    if (unlikely(parent->sb->flags & MS_RDONLY))
        return errno = EROFS, -1;

    ext2_t* ext2      = parent->sb->fsinfo;
    size_t name_len   = strlen(name);
    size_t target_len = strlen(target);

    if (unlikely(name_len == 0 || name_len > EXT2_NAME_LEN))
        return errno = ENAMETOOLONG, -1;

    if (unlikely(target_len == 0))
        return errno = ENOENT, -1;

    if (unlikely(target_len >= ext2->blocksize))
        return errno = ENAMETOOLONG, -1;

    inode_t* existing = ext2_finddir(parent, name);

    if (existing)
        return kfree(existing), errno = EEXIST, -1;

    if (errno != ENOENT)
        return -1;

    struct ext2_inode* dir     = cache_get(&parent->sb->cache, parent->ino);
    struct ext2_inode* created = kcalloc(1, ext2->inodesize, GFP_KERNEL);
    ino_t ino                  = 0;
    int result                 = -1;

    if (unlikely(!created))
        return errno = ENOMEM, -1;

    created->i_mode        = S_IFLNK | 0777;
    created->i_uid         = current_task->fsuid;
    created->i_gid         = current_task->fsgid;
    created->i_links_count = 1;
    created->i_size        = target_len;
    created->i_atime       = arch_timer_gettime();
    created->i_ctime       = created->i_atime;
    created->i_mtime       = created->i_atime;

    if (ext2_inode_is_fast_symlink(created))
        memcpy(created->i_block, target, target_len);

    scoped_lock(&ext2->lock) {
        if (ext2_utils_alloc_inode(ext2, created, &ino) < 0)
            break;

        if (!ext2_inode_is_fast_symlink(created)) {
            if (ext2_utils_write_inode_data(ext2, created, 0, 0, target, target_len) < 0 || ext2_utils_write_inode(ext2, ino, created) < 0) {
                ext2_symlink_rollback(ext2, created, ino);
                break;
            }
        }

        if (ext2_utils_add_dir_entry(ext2, dir, ino, name, created->i_mode) < 0) {
            ext2_symlink_rollback(ext2, created, ino);
            break;
        }

        dir->i_mtime = arch_timer_gettime();
        dir->i_ctime = dir->i_mtime;

        if (ext2_utils_write_inode(ext2, parent->ino, dir) < 0)
            break;

        result = 0;
    }

    kfree(created);
    return result;
}
