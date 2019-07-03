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

#include <sys/types.h>
#include <sys/mount.h>

#include <aplus/utils/list.h>

#include "tmpfs.h"



__thread_safe
int tmpfs_mount(inode_t* dev, inode_t* dir, int flags, const char * args) {
    
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dir->ino);
    DEBUG_ASSERT(dev == NULL);

    (void) args;
    (void) dev;

    DEBUG_ASSERT(S_ISDIR(dir->ino->st.st_mode));



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



    __lock(&dir->lock, {

        dir->sb = PTR_REF(&dir->__sb);
        dir->sb->fsid = TMPFS_ID;
        dir->sb->dev = dev;
        dir->sb->root = dir;
        dir->sb->flags = flags;
        dir->sb->cache = NULL;
        
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
        dir->sb->st.f_namemax = MAXNAMLEN;



        dir->sb->ops.finddir = tmpfs_finddir;
        dir->sb->ops.readdir = tmpfs_readdir;
        dir->sb->ops.mknod = tmpfs_mknod;
        dir->sb->ops.unlink = tmpfs_unlink;


        dir->ino->st.st_mode &= ~S_IFMT;
        dir->ino->st.st_mode |=  S_IFMT;

    });

    return 0;
}