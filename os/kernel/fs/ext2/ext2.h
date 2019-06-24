/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _TMPFS_H
#define _TMPFS_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>


#define EXT2_SIGNATURE                  0xEF53
#define EXT2_SECTOR_SIZE                512

#define INODE_TYPE_FIFO                 0x1000
#define INODE_TYPE_CHAR_DEV             0x2000
#define INODE_TYPE_DIRECTORY            0x4000
#define INODE_TYPE_BLOCK_DEV            0x6000
#define INODE_TYPE_FILE                 0x8000
#define INODE_TYPE_SYMLINK              0xA000
#define INODE_TYPE_SOCKET               0xC000





#define EXT2(i)                             \
    ((ext2_t*)                              \
    (((i->st.st_mode & S_IFMT) == S_IFMT)   \
        ? i->mount.userdata                 \
        : i->root->mount.userdata)          \
    )




typedef struct {
    uint32_t inodes;
    uint32_t blocks;
    uint32_t reserved_for_root;
    uint32_t unallocatedblocks;
    uint32_t unallocatedinodes;
    uint32_t superblock_id;
    uint32_t blocksize_hint;
    uint32_t fragmentsize_hint;
    uint32_t blocks_in_blockgroup;
    uint32_t frags_in_blockgroup;
    uint32_t inodes_in_blockgroup;
    uint32_t last_mount;
    uint32_t last_write;
    uint16_t mounts_since_last_check;
    uint16_t max_mounts_since_last_check;
    uint16_t ext2_sig;
    uint16_t state;
    uint16_t op_on_err;
    uint16_t minor_version;
    uint32_t last_check;
    uint32_t max_time_in_checks;
    uint32_t os_id;
    uint32_t major_version;
    uint16_t uuid;
    uint16_t gid;
    uint8_t unused[940];
} __packed superblock_t;






inode_t* ext2_finddir (inode_t*, const char*);
inode_t* ext2_mknod (inode_t*, const char* name, mode_t mode);
int ext2_getdents (inode_t*, struct dirent*, off_t, size_t);
int ext2_unlink (inode_t*, const char* name);
int ext2_read(inode_t*, void __user *, off_t, size_t);
int ext2_write(inode_t*, const void __user *, off_t, size_t);

#endif