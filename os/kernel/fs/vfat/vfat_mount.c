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

#include "vfat.h"


__thread_safe
int vfat_mount(inode_t* dev, inode_t* dir, int flags, const char* args) {
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






    __lock(&dir->lock, {

        dir->mount.type = "vfat";
        dir->mount.dev = dev;
        dir->mount.flags = flags;
        
        //dir->mount.userdata = (void*) kcalloc(1, sizeof(tmpfs_t), GFP_USER);

        
        dir->mount.st.f_bsize = 0;
        dir->mount.st.f_frsize = 0;
        dir->mount.st.f_blocks = 0;
        dir->mount.st.f_bfree = 0;
        dir->mount.st.f_bavail = 0;
        dir->mount.st.f_files = 0;
        dir->mount.st.f_ffree = 0;
        dir->mount.st.f_favail = 0;
        dir->mount.st.f_flag = stflags;
        dir->mount.st.f_fsid = VFAT_ID;
        dir->mount.st.f_namemax = MAXNAMLEN;



        //dir->mount.ops.finddir = vfat_finddir;
        //dir->mount.ops.getdents = vfat_getdents;
        //dir->mount.ops.mknod = vfat_mknod;
        //dir->mount.ops.unlink = vfat_unlink;


        dir->st.st_mode &= ~S_IFMT;
        dir->st.st_mode |=  S_IFMT;

    });

    return 0;
}