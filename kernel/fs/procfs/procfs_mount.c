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
#include <sys/mount.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "procfs.h"


int procfs_mount(inode_t* dev, inode_t* dir, int flags, const char* args) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dev == NULL);

    (void)args;
    (void)dev;


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
    __(MS_RELATIME, ST_RELATIME);
    __(MS_SYNCHRONOUS, ST_SYNCHRONOUS);

#undef __


    dir->sb = (struct superblock*)kcalloc(sizeof(struct superblock), 1, GFP_KERNEL);

    dir->sb->fsid  = FSID_PROCFS;
    dir->sb->dev   = dev;
    dir->sb->root  = dir;
    dir->sb->ino   = dir->ino;
    dir->sb->flags = stflags;


    dir->sb->st.f_bsize   = 1;
    dir->sb->st.f_frsize  = 1;
    dir->sb->st.f_blocks  = 0;
    dir->sb->st.f_bfree   = 0;
    dir->sb->st.f_bavail  = 0;
    dir->sb->st.f_files   = 0;
    dir->sb->st.f_ffree   = 0;
    dir->sb->st.f_favail  = 0;
    dir->sb->st.f_fsid    = FSID_PROCFS;
    dir->sb->st.f_flag    = ST_SYNCHRONOUS | ST_NODEV | ST_NOEXEC | ST_NOSUID | stflags;
    dir->sb->st.f_namemax = CONFIG_MAXNAMLEN;


    // dir->sb->ops.getattr = procfs_getattr;
    dir->sb->ops.finddir = procfs_root_finddir;
    dir->sb->ops.readdir = procfs_root_readdir;

    dir->sb->flags |= INODE_FLAGS_DCACHE_DISABLED;


    procfs_service_pid_init(dir);

    return 0;
}
