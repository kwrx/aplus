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

#include <stdint.h>
#include <sys/types.h>
#include <sys/mount.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>

#include <aplus/utils/list.h>

#include "tmpfs.h"




int tmpfs_mount(inode_t* dev, inode_t* dir, int flags, const char * args) {
    
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dev == NULL);

    (void) args;
    (void) dev;


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



    dir->sb = (struct superblock*) kcalloc(sizeof(struct superblock), 1, GFP_KERNEL);

    dir->sb->fsid = TMPFS_ID;
    dir->sb->dev = dev;
    dir->sb->root = dir;
    dir->sb->flags = flags;

    dir->sb->fsinfo = (void*) kcalloc(1, sizeof(tmpfs_t), GFP_USER);


    dir->sb->st.f_bsize = 1;
    dir->sb->st.f_frsize = 1;
    dir->sb->st.f_blocks = TMPFS_SIZE_MAX;
    dir->sb->st.f_bfree = TMPFS_SIZE_MAX;
    dir->sb->st.f_bavail = TMPFS_SIZE_MAX;
    dir->sb->st.f_files = 0;
    dir->sb->st.f_ffree = TMPFS_NODES_MAX;
    dir->sb->st.f_favail = TMPFS_NODES_MAX;
    dir->sb->st.f_flag = ST_SYNCHRONOUS | ST_NODEV | stflags;
    dir->sb->st.f_fsid = TMPFS_ID;
    dir->sb->st.f_namemax = CONFIG_MAXNAMLEN;


    dir->sb->ops.getattr = tmpfs_getattr;
    dir->sb->ops.setattr = tmpfs_setattr;
    dir->sb->ops.creat = tmpfs_creat;
    dir->sb->ops.finddir = tmpfs_finddir;
    dir->sb->ops.readdir = tmpfs_readdir;
    dir->sb->ops.rename = tmpfs_rename;
    dir->sb->ops.symlink = tmpfs_symlink;
    dir->sb->ops.unlink = tmpfs_unlink;



    struct vfs_cache_ops ops;
    ops.flush = tmpfs_cache_flush;
    ops.load  = tmpfs_cache_load;

    vfs_cache_create(&dir->sb->cache, &ops, TMPFS_NODES_MAX, NULL);


    dir->sb->ino = dir->ino;

    return 0;
}