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

#include "../ext2.h"


mode_t ext2_utils_file_type(uint8_t type) {
    switch (type) {
        case EXT2_FT_REG_FILE:
            return S_IFREG;
        case EXT2_FT_DIR:
            return S_IFDIR;
        case EXT2_FT_CHRDEV:
            return S_IFCHR;
        case EXT2_FT_BLKDEV:
            return S_IFBLK;
        case EXT2_FT_FIFO:
            return S_IFIFO;
        case EXT2_FT_SOCK:
            return S_IFSOCK;
        case EXT2_FT_SYMLINK:
            return S_IFLNK;
        case EXT2_FT_UNKNOWN:
            return 0;
        default:
            return 0;
    }
}


uint8_t ext2_utils_dir_type(mode_t mode) {
    if (S_ISREG(mode))
        return EXT2_FT_REG_FILE;
    if (S_ISDIR(mode))
        return EXT2_FT_DIR;
    if (S_ISCHR(mode))
        return EXT2_FT_CHRDEV;
    if (S_ISBLK(mode))
        return EXT2_FT_BLKDEV;
    if (S_ISFIFO(mode))
        return EXT2_FT_FIFO;
    if (S_ISSOCK(mode))
        return EXT2_FT_SOCK;
    if (S_ISLNK(mode))
        return EXT2_FT_SYMLINK;

    return EXT2_FT_UNKNOWN;
}


static void ext2_fill_dir_entry(struct ext2_dir_entry_2* entry, uint16_t rec_len, ino_t ino, const char* name, mode_t mode) {
    size_t name_len = strlen(name);

    memset(entry, 0, rec_len);
    entry->inode     = ino;
    entry->rec_len   = rec_len;
    entry->name_len  = name_len;
    entry->file_type = ext2_utils_dir_type(mode);
    memcpy(entry->name, name, name_len);
}


int ext2_utils_add_dir_entry(ext2_t* ext2, struct ext2_inode* dir, ino_t ino, const char* name, mode_t mode) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(name);

    size_t name_len = strlen(name);

    if (unlikely(name_len == 0 || name_len > EXT2_NAME_LEN))
        return errno = ENAMETOOLONG, -1;

    if (unlikely(dir->i_flags & EXT2_INDEX_FL))
        return errno = EOPNOTSUPP, -1;

    uint16_t needed = EXT2_DIR_REC_LEN(name_len);
    uint32_t blocks = (dir->i_size + ext2->blocksize - 1) / ext2->blocksize;

    for (uint32_t block = 0; block < blocks; block++) {
        if (ext2_utils_read_inode_data(ext2, dir, block, 0, ext2->iocache, ext2->blocksize) < 0)
            return -1;

        for (uint32_t offset = 0; offset < ext2->blocksize;) {
            struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)((uint8_t*)ext2->iocache + offset);

            if (unlikely(entry->rec_len < 8 || entry->rec_len > ext2->blocksize - offset || entry->name_len > entry->rec_len - 8))
                return errno = EIO, -1;

            if (entry->inode == 0 && entry->rec_len >= needed) {
                ext2_fill_dir_entry(entry, entry->rec_len, ino, name, mode);
                return ext2_utils_write_inode_data(ext2, dir, block, 0, ext2->iocache, ext2->blocksize);
            }

            uint16_t actual = EXT2_DIR_REC_LEN(entry->name_len);

            if (entry->inode != 0 && entry->rec_len >= actual + needed) {
                uint16_t available = entry->rec_len - actual;
                entry->rec_len     = actual;

                struct ext2_dir_entry_2* added = (struct ext2_dir_entry_2*)((uint8_t*)entry + actual);
                ext2_fill_dir_entry(added, available, ino, name, mode);

                return ext2_utils_write_inode_data(ext2, dir, block, 0, ext2->iocache, ext2->blocksize);
            }

            offset += entry->rec_len;
        }
    }

    uint8_t buffer[EXT2_MAX_BLOCK_SIZE] = {0};
    ext2_fill_dir_entry((struct ext2_dir_entry_2*)buffer, ext2->blocksize, ino, name, mode);

    if (ext2_utils_write_inode_data(ext2, dir, blocks, 0, buffer, ext2->blocksize) < 0)
        return -1;

    dir->i_size += ext2->blocksize;
    return 0;
}


int ext2_utils_remove_dir_entry(ext2_t* ext2, struct ext2_inode* dir, const char* name, ino_t* ino, uint8_t* type) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(ino);
    DEBUG_ASSERT(type);

    size_t name_len = strlen(name);

    if (unlikely(dir->i_flags & EXT2_INDEX_FL))
        return errno = EOPNOTSUPP, -1;

    uint32_t blocks = (dir->i_size + ext2->blocksize - 1) / ext2->blocksize;

    for (uint32_t block = 0; block < blocks; block++) {
        if (ext2_utils_read_inode_data(ext2, dir, block, 0, ext2->iocache, ext2->blocksize) < 0)
            return -1;

        struct ext2_dir_entry_2* previous = NULL;

        for (uint32_t offset = 0; offset < ext2->blocksize;) {
            struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)((uint8_t*)ext2->iocache + offset);

            if (unlikely(entry->rec_len < 8 || entry->rec_len > ext2->blocksize - offset || entry->name_len > entry->rec_len - 8))
                return errno = EIO, -1;

            if (entry->inode != 0 && entry->name_len == name_len && memcmp(entry->name, name, name_len) == 0) {
                *ino  = entry->inode;
                *type = entry->file_type;

                if (previous) {
                    previous->rec_len += entry->rec_len;
                } else {
                    entry->inode     = 0;
                    entry->name_len  = 0;
                    entry->file_type = EXT2_FT_UNKNOWN;
                }

                return ext2_utils_write_inode_data(ext2, dir, block, 0, ext2->iocache, ext2->blocksize);
            }

            previous = entry;

            offset += entry->rec_len;
        }
    }

    return errno = ENOENT, -1;
}


/**
 * @brief Writes the first block of a new directory, holding its "." and ".." entries, and records it in the inode.
 * @param ext2 The filesystem.
 * @param node The new directory, updated in memory and on disk.
 * @param ino Its inode number.
 * @param parent The inode number of its parent.
 * @return 0 on success, -1 with errno set.
 */
int ext2_utils_init_dir(ext2_t* ext2, struct ext2_inode* node, ino_t ino, ino_t parent) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(node);

    uint8_t* block = kcalloc(1, ext2->blocksize, GFP_KERNEL);

    if (unlikely(!block))
        return errno = ENOMEM, -1;

    struct ext2_dir_entry_2* dot = (struct ext2_dir_entry_2*)block;
    ext2_fill_dir_entry(dot, EXT2_DIR_REC_LEN(1), ino, ".", S_IFDIR);

    struct ext2_dir_entry_2* dotdot = (struct ext2_dir_entry_2*)(block + dot->rec_len);
    ext2_fill_dir_entry(dotdot, ext2->blocksize - dot->rec_len, parent, "..", S_IFDIR);

    int result = ext2_utils_write_inode_data(ext2, node, 0, 0, block, ext2->blocksize);

    kfree(block);

    if (result < 0)
        return -1;

    node->i_size = ext2->blocksize;

    return ext2_utils_write_inode(ext2, ino, node);
}


/**
 * @brief Points the ".." entry of a directory at a new parent.
 * @param ext2 The filesystem.
 * @param node The directory.
 * @param parent The inode number of its new parent.
 * @return 0 on success, -1 with errno set.
 */
int ext2_utils_set_dir_parent(ext2_t* ext2, struct ext2_inode* node, ino_t parent) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(node);

    if (ext2_utils_read_inode_data(ext2, node, 0, 0, ext2->iocache, ext2->blocksize) < 0)
        return -1;

    for (uint32_t offset = 0; offset < ext2->blocksize;) {
        struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)((uint8_t*)ext2->iocache + offset);

        if (unlikely(entry->rec_len < 8 || entry->rec_len > ext2->blocksize - offset || entry->name_len > entry->rec_len - 8))
            return errno = EIO, -1;

        if (entry->inode != 0 && entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.') {
            entry->inode = parent;
            return ext2_utils_write_inode_data(ext2, node, 0, 0, ext2->iocache, ext2->blocksize);
        }

        offset += entry->rec_len;
    }

    return errno = EIO, -1;
}


/**
 * @brief Tells whether a directory holds nothing but its "." and ".." entries.
 * @param ext2 The filesystem.
 * @param node The directory.
 * @return 1 when it is empty, 0 when it is not, -1 with errno set on error.
 */
int ext2_utils_dir_empty(ext2_t* ext2, struct ext2_inode* node) {
    DEBUG_ASSERT(ext2);
    DEBUG_ASSERT(node);

    uint32_t blocks = (node->i_size + ext2->blocksize - 1) / ext2->blocksize;

    for (uint32_t block = 0; block < blocks; block++) {
        if (ext2_utils_read_inode_data(ext2, node, block, 0, ext2->iocache, ext2->blocksize) < 0)
            return -1;

        for (uint32_t offset = 0; offset < ext2->blocksize;) {
            struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)((uint8_t*)ext2->iocache + offset);

            if (unlikely(entry->rec_len < 8 || entry->rec_len > ext2->blocksize - offset || entry->name_len > entry->rec_len - 8))
                return errno = EIO, -1;

            bool dots = (entry->name_len == 1 && entry->name[0] == '.') || (entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.');

            if (entry->inode != 0 && !dots)
                return 0;

            offset += entry->rec_len;
        }
    }

    return 1;
}
