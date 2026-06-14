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
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include "ext2.h"


inode_t* ext2_utils_create_vfs_inode(inode_t* parent, ino_t ino, const char* name, mode_t mode) {
    inode_t* inode = (inode_t*)kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!inode))
        return errno = ENOMEM, NULL;

    inode->ino    = ino;
    inode->sb     = parent->sb;
    inode->parent = parent;

    strncpy(inode->name, name, sizeof(inode->name) - 1);
    spinlock_init(&inode->lock);

    inode->ops.getattr = ext2_getattr;
    inode->ops.setattr = ext2_setattr;
    inode->ops.fsync   = ext2_fsync;

    if (S_ISDIR(mode)) {
        inode->ops.creat   = ext2_creat;
        inode->ops.finddir = ext2_finddir;
        inode->ops.readdir = ext2_readdir;
        inode->ops.unlink  = ext2_unlink;
        vfs_dcache_init(inode);
    } else if (S_ISREG(mode)) {
        inode->ops.truncate = ext2_truncate;
        inode->ops.read     = ext2_read;
        inode->ops.write    = ext2_write;
    } else if (S_ISLNK(mode)) {
        inode->ops.readlink = ext2_readlink;
    }

    return inode;
}


inode_t* ext2_finddir(inode_t* inode, const char* name) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(name);

    size_t name_len = strlen(name);

    if (unlikely(name_len == 0 || name_len > EXT2_NAME_LEN))
        return errno = ENOENT, NULL;

    ext2_t* ext2            = inode->sb->fsinfo;
    struct ext2_inode* node = cache_get(&inode->sb->cache, inode->ino);
    inode_t* result         = NULL;

    errno = 0;

    scoped_lock(&ext2->lock) {
        for (uint32_t block = 0; (uint64_t)block * ext2->blocksize < node->i_size; block++) {
            if (ext2_utils_read_inode_data(ext2, node, block, 0, ext2->iocache, ext2->blocksize) < 0)
                break;

            for (uint32_t offset = 0; offset < ext2->blocksize;) {
                struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)((uint8_t*)ext2->iocache + offset);

                if (unlikely(entry->rec_len < 8 || entry->rec_len > ext2->blocksize - offset || entry->name_len > entry->rec_len - 8)) {
                    errno = EIO;
                    break;
                }

                if (entry->inode != 0 && entry->name_len == name_len && memcmp(name, entry->name, name_len) == 0) {
                    mode_t mode;

                    if (entry->file_type != EXT2_FT_UNKNOWN) {
                        mode = ext2_utils_file_type(entry->file_type);
                    } else {
                        struct ext2_inode* child = cache_get(&inode->sb->cache, entry->inode);
                        mode                     = child->i_mode;
                    }

                    result = ext2_utils_create_vfs_inode(inode, entry->inode, name, mode);
                    break;
                }

                offset += entry->rec_len;
            }

            if (result)
                break;
        }
    }

    if (!result)
        errno = errno == EIO ? EIO : ENOENT;

    return result;
}
