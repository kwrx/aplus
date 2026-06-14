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


ssize_t ext2_readdir(inode_t* inode, struct dirent* entries, off_t pos, size_t count) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_EXT2);
    DEBUG_ASSERT(entries);

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;

    if (count == 0)
        return 0;

    ext2_t* ext2            = inode->sb->fsinfo;
    struct ext2_inode* node = cache_get(&inode->sb->cache, inode->ino);
    size_t seen             = 0;
    size_t emitted          = 0;

    scoped_lock(&ext2->lock) {
        for (uint32_t block = 0; (uint64_t)block * ext2->blocksize < node->i_size && emitted < count; block++) {
            if (ext2_utils_read_inode_data(ext2, node, block, 0, ext2->iocache, ext2->blocksize) < 0)
                return emitted ? (ssize_t)emitted : -1;

            for (uint32_t offset = 0; offset < ext2->blocksize && emitted < count;) {
                struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)((uint8_t*)ext2->iocache + offset);

                if (unlikely(entry->rec_len < 8 || entry->rec_len > ext2->blocksize - offset || entry->name_len > entry->rec_len - 8))
                    return errno = EIO, emitted ? (ssize_t)emitted : -1;

                if (entry->inode != 0) {
                    if (seen++ >= (size_t)pos) {
                        struct dirent* out = &entries[emitted++];

                        memset(out, 0, sizeof(*out));
                        out->d_ino    = entry->inode;
                        out->d_off    = pos + emitted;
                        out->d_reclen = sizeof(*out);
                        out->d_type   = MODE_2_DIRENT_TYPE(ext2_utils_file_type(entry->file_type));

                        size_t name_len = entry->name_len;
                        if (name_len >= sizeof(out->d_name))
                            name_len = sizeof(out->d_name) - 1;

                        memcpy(out->d_name, entry->name, name_len);
                    }
                }

                offset += entry->rec_len;
            }
        }
    }

    return emitted;
}
