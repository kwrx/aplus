/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include <stdint.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>

#include "ext2.h"



int ext2_mount(inode_t* dev, inode_t* dir, int flags, const char* args) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dev);

    (void)args;


    // TODO: ext2: include support for fsync and atime
    flags |= MS_SYNCHRONOUS;
    flags |= MS_NOATIME;

#define __(a, b)   \
    if (flags & a) \
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



    struct ext2_super_block sb = {0};

    if (vfs_read(dev, &sb, 1024, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) {

#if DEBUG_LEVEL_ERROR
        kprintf("ext2: ERROR! vfs_read() error(%d)\n", errno);
#endif

        return -1;
    }


    if (sb.s_magic != EXT2_SIGNATURE) {

#if DEBUG_LEVEL_ERROR
        kprintf("ext2: ERROR! invalid signature, no ext2 filesystem: %X\n", sb.s_magic);
#endif

        return errno = EINVAL, -1;
    }



    DEBUG_ASSERT((1024 << sb.s_log_block_size) <= EXT2_MAX_BLOCK_SIZE);
    DEBUG_ASSERT((1024 << sb.s_log_block_size) >= EXT2_MIN_BLOCK_SIZE);
    DEBUG_ASSERT((1024 << sb.s_log_frag_size) <= EXT2_MAX_FRAG_SIZE);
    DEBUG_ASSERT((1024 << sb.s_log_frag_size) >= EXT2_MIN_FRAG_SIZE);

    DEBUG_ASSERT(!(sb.s_feature_incompat & EXT2_FEATURE_INCOMPAT_COMPRESSION));
    DEBUG_ASSERT(!(sb.s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER));
    DEBUG_ASSERT(!(sb.s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV));

    //! WARN
    DEBUG_ASSERT(sb.s_rev_level == EXT2_DYNAMIC_REV);



    ext2_t* ext2 = (void*)kcalloc(1, sizeof(ext2_t), GFP_USER);

    ext2->iocache           = (void*)kmalloc((1024) << sb.s_log_block_size, GFP_KERNEL);
    ext2->first_block_group = sb.s_first_data_block + 1;
    ext2->count_block_group = sb.s_blocks_count / sb.s_blocks_per_group;
    ext2->blocksize         = (1024) << sb.s_log_block_size;
    ext2->inodesize         = sb.s_inode_size;
    ext2->dev               = dev;
    ext2->root              = dir;

    memcpy(&ext2->sb, &sb, sizeof(struct ext2_super_block));


    spinlock_init(&ext2->lock);



    dir->sb = (struct superblock*)kcalloc(sizeof(struct superblock), 1, GFP_KERNEL);

    dir->sb->fsid  = FSID_EXT2;
    dir->sb->dev   = dev;
    dir->sb->root  = dir;
    dir->sb->flags = flags;


    dir->sb->fsinfo = (void*)ext2;

    dir->sb->st.f_bsize   = 1024 << sb.s_log_block_size;
    dir->sb->st.f_frsize  = 1024 << sb.s_log_frag_size;
    dir->sb->st.f_blocks  = sb.s_blocks_count;
    dir->sb->st.f_bfree   = sb.s_free_blocks_count;
    dir->sb->st.f_bavail  = sb.s_free_blocks_count;
    dir->sb->st.f_files   = sb.s_inodes_count;
    dir->sb->st.f_ffree   = sb.s_free_inodes_count;
    dir->sb->st.f_favail  = sb.s_free_inodes_count;
    dir->sb->st.f_flag    = stflags;
    dir->sb->st.f_fsid    = FSID_EXT2;
    dir->sb->st.f_namemax = CONFIG_MAXNAMLEN;

    dir->sb->ops.getattr = ext2_getattr;
    // dir->sb->ops.setattr = ext2_setattr;
    // dir->sb->ops.creat = ext2_creat;
    dir->sb->ops.finddir = ext2_finddir;
    dir->sb->ops.readdir = ext2_readdir;
    // dir->sb->ops.rename = ext2_rename;
    // dir->sb->ops.symlink = ext2_symlink;
    // dir->sb->ops.unlink = ext2_unlink;



    struct cache_ops iops;
    iops.fetch   = (cache_fetch_handler_t)ext2_icache_fetch;
    iops.commit  = (cache_commit_handler_t)ext2_icache_commit;
    iops.release = (cache_release_handler_t)ext2_icache_release;

    cache_init(&dir->sb->cache, &iops, SIZE_MAX, ext2);


    struct cache_ops bops;
    bops.fetch   = (cache_fetch_handler_t)ext2_bcache_fetch;
    bops.commit  = (cache_commit_handler_t)ext2_bcache_commit;
    bops.release = (cache_release_handler_t)ext2_bcache_release;

    cache_init(&ext2->bcache, &bops, SIZE_MAX, ext2);



    dir->sb->ino = EXT2_ROOT_INO;

    return 0;
}
