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


inode_t* ext2_creat(inode_t* parent, const char* name, mode_t mode) {
    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(name);

    if (unlikely(parent->sb->flags & MS_RDONLY))
        return errno = EROFS, NULL;

    if (unlikely(!S_ISREG(mode)))
        return errno = EOPNOTSUPP, NULL;

    size_t name_len = strlen(name);

    if (unlikely(name_len == 0 || name_len > EXT2_NAME_LEN))
        return errno = ENAMETOOLONG, NULL;

    ext2_t* ext2               = parent->sb->fsinfo;
    struct ext2_inode* dir     = cache_get(&parent->sb->cache, parent->ino);
    struct ext2_inode* created = kcalloc(1, ext2->inodesize, GFP_KERNEL);
    ino_t ino                  = 0;
    inode_t* result            = NULL;

    if (unlikely(!created))
        return errno = ENOMEM, NULL;

    mode_t umask = 0;
    shared_ptr_access(current_task->fs, fs, { umask = fs->umask; });

    created->i_mode        = mode & ~umask;
    created->i_uid         = current_task->uid;
    created->i_gid         = current_task->gid;
    created->i_links_count = 1;
    created->i_atime       = arch_timer_gettime();
    created->i_ctime       = created->i_atime;
    created->i_mtime       = created->i_atime;

    scoped_lock(&ext2->lock) {
        if (ext2_utils_alloc_inode(ext2, created, &ino) < 0)
            break;

        if (ext2_utils_add_dir_entry(ext2, dir, ino, name, created->i_mode) < 0) {
            ext2_utils_free_inode(ext2, ino);
            ino = 0;
            break;
        }

        dir->i_mtime = arch_timer_gettime();
        dir->i_ctime = dir->i_mtime;

        if (ext2_utils_write_inode(ext2, parent->ino, dir) < 0)
            break;

        result = ext2_utils_create_vfs_inode(parent, ino, name, created->i_mode);
    }

    kfree(created);
    return result;
}
