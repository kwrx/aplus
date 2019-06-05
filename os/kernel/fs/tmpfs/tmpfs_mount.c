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




int tmpfs_mount(inode_t* dev, inode_t* dir, int flags, const char __user * args) {
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dev == NULL);

    (void) args;
    (void) dev;

    DEBUG_ASSERT(S_ISDIR(dir->st.st_mode));



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

        dir->mount.type = "tmpfs";
        dir->mount.dev = NULL;
        dir->mount.flags = flags;
        
        dir->mount.userdata = (void*) kcalloc(1, sizeof(tmpfs_t), GFP_USER);

        
        dir->mount.st.f_bsize = 1;
        dir->mount.st.f_frsize = 1;
        dir->mount.st.f_blocks = TMPFS_SIZE_MAX;
        dir->mount.st.f_bfree = TMPFS_SIZE_MAX;
        dir->mount.st.f_bavail = TMPFS_SIZE_MAX;
        dir->mount.st.f_files = 0;
        dir->mount.st.f_ffree = TMPFS_NODES_MAX;
        dir->mount.st.f_favail = TMPFS_NODES_MAX;
        dir->mount.st.f_flag = ST_SYNCHRONOUS | ST_NODEV | stflags;
        dir->mount.st.f_fsid = TMPFS_MAGIC;
        dir->mount.st.f_namemax = MAXNAMLEN;



        dir->mount.ops.finddir = tmpfs_finddir;
        dir->mount.ops.getdents = tmpfs_getdents;
        dir->mount.ops.mknod = tmpfs_mknod;
        dir->mount.ops.unlink = tmpfs_unlink;


        dir->st.st_mode &= ~S_IFMT;
        dir->st.st_mode |=  S_IFMT;

    });

    return 0;
}