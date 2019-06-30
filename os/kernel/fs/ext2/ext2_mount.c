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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/mount.h>

#include "ext2.h"


__thread_safe
int ext2_mount(inode_t* dev, inode_t* dir, int flags, const char* args) {
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(S_ISDIR(dir->st.st_mode));

    (void) args;



    #define __(a, b)                        \
        if(flags & a)                       \
            stflags |= b

        int stflags = 0;

        __(MS_MANDLOCK, ST_MANDLOCK);
        __(MS_NOATIME, ST_NOATIME);
        __(MS_NODEV, ST_NODEV);
        __(MS_NODIRATIME, ST_NODIRATIME);
        __(MS_NOEXEC, ST_NOEXEC);
        __(MS_NOSUID, ST_NOSUID);
        __(MS_RDONLY, ST_RDONLY);
        __(MS_SYNCHRONOUS, ST_SYNCHRONOUS);

    #undef __




    struct ext2_super_block sb;
    if(vfs_read(dev, &sb, 1024, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block))
        return kprintf("ext2: ERROR! vfs_read() error: %s\n", strerror(errno)), -1;


    if(sb.s_magic != EXT2_SIGNATURE)
        return kprintf("ext2: ERROR! invalid signature, no ext2 filesystem: %p\n", sb.s_magic),
            errno = EINVAL, -1;



    DEBUG_ASSERT((1024 << sb.s_log_block_size) <= EXT2_MAX_BLOCK_SIZE);
    DEBUG_ASSERT((1024 << sb.s_log_block_size) >= EXT2_MIN_BLOCK_SIZE);
    DEBUG_ASSERT((1024 << sb.s_log_frag_size)  <= EXT2_MAX_FRAG_SIZE);
    DEBUG_ASSERT((1024 << sb.s_log_frag_size)  >= EXT2_MIN_FRAG_SIZE);

    DEBUG_ASSERT(!(sb.s_feature_incompat & EXT2_FEATURE_INCOMPAT_COMPRESSION));
    DEBUG_ASSERT(!(sb.s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER));
    DEBUG_ASSERT(!(sb.s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV));



    __lock(&dir->lock, {

        dir->mount.type = "ext2";
        dir->mount.dev = dev;
        dir->mount.flags = flags;

        vfs_cache_create(&dir->mount.cache, 1024);

        
        ext2_t* ext2 = (void*) kcalloc(1, sizeof(ext2_t), GFP_USER);   

        ext2->cache = (void*) kmalloc((1024) << sb.s_log_block_size, GFP_KERNEL);        
        ext2->first_block_group = sb.s_first_data_block + 1;
        ext2->count_block_group = sb.s_blocks_count / sb.s_blocks_per_group;
        ext2->blocksize = (1024) << sb.s_log_block_size;
        ext2->inodesize = sb.s_inode_size;
        ext2->dev = dev;
        ext2->root = dir;

        memcpy(&ext2->sb, &sb, sizeof(struct ext2_super_block));
        spinlock_init(&ext2->lock);



        dir->fsinfo = (void*) ext2;
        
        dir->mount.st.f_bsize = 1024 << sb.s_log_block_size;
        dir->mount.st.f_frsize = 1024 << sb.s_log_frag_size;
        dir->mount.st.f_blocks = sb.s_blocks_count;
        dir->mount.st.f_bfree = sb.s_free_blocks_count;
        dir->mount.st.f_bavail = sb.s_free_blocks_count;
        dir->mount.st.f_files = sb.s_inodes_count;
        dir->mount.st.f_ffree = sb.s_free_inodes_count;
        dir->mount.st.f_favail = sb.s_free_inodes_count;
        dir->mount.st.f_flag = stflags;
        dir->mount.st.f_fsid = EXT2_ID;
        dir->mount.st.f_namemax = MAXNAMLEN;



        dir->mount.ops.finddir = ext2_finddir;
        dir->mount.ops.readdir = ext2_readdir;
        dir->mount.ops.mknod = ext2_mknod;
        dir->mount.ops.unlink = ext2_unlink;

        dir->st.st_ino = EXT2_ROOT_INO;
        dir->st.st_mode &= ~S_IFMT;
        dir->st.st_mode |=  S_IFMT;


    });


    return 0;
}