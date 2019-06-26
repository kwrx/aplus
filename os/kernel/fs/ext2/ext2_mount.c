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

#include <aplus/utils/list.h>

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




    superblock_t sb;
    if(vfs_read(dev, &sb, 1024, sizeof(superblock_t)) != sizeof(superblock_t))
        return kprintf("ext2: ERROR! vfs_read() error: %s\n", strerror(errno)), -1;


    if(sb.ext2_sig != EXT2_SIGNATURE)
        return kprintf("ext2: ERROR! invalid signature, no ext2 filesystem\n"),
               errno = EINVAL, -1;


    

#if 0

    kprintf(
        "ext2: superblock\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n"
        "       .%-28s: %p\n",

        #define _(p) \
            #p, sb.p

        _(inodes),
        _(blocks),
        _(reserved_for_root),
        _(unallocated_blocks),
        _(unallocated_inodes),
        _(superblock_id),
        _(blocksize_hint),
        _(fragmentsize_hint),
        _(blocks_in_blockgroup),
        _(frags_in_blockgroup),
        _(inodes_in_blockgroup),
        _(last_mount),
        _(last_write),
        _(mounts_since_last_check),
        _(max_mounts_since_last_check),
        _(ext2_sig),
        _(state),
        _(op_on_err),
        _(minor_version),
        _(last_check),
        _(max_time_in_checks),
        _(os_id),
        _(major_version),
        _(uuid),
        _(gid)

        #undef _
    );

#endif
    




    __lock(&dir->lock, {

        dir->mount.type = "ext2";
        dir->mount.dev = dev;
        dir->mount.flags = flags;
        
        //dir->mount.userdata = (void*) kcalloc(1, sizeof(tmpfs_t), GFP_USER);

        
        dir->mount.st.f_bsize = 1024 << sb.blocksize_hint;
        dir->mount.st.f_frsize = 1024 << sb.fragmentsize_hint;
        dir->mount.st.f_blocks = sb.blocks;
        dir->mount.st.f_bfree = sb.unallocated_blocks;
        dir->mount.st.f_bavail = sb.unallocated_blocks;
        dir->mount.st.f_files = sb.inodes;
        dir->mount.st.f_ffree = sb.unallocated_inodes;
        dir->mount.st.f_favail = sb.unallocated_inodes;
        dir->mount.st.f_flag = stflags;
        dir->mount.st.f_fsid = EXT2_SUPER_MAGIC;
        dir->mount.st.f_namemax = MAXNAMLEN;



        dir->mount.ops.finddir = ext2_finddir;
        dir->mount.ops.getdents = ext2_getdents;
        dir->mount.ops.mknod = ext2_mknod;
        dir->mount.ops.unlink = ext2_unlink;


        dir->st.st_mode &= ~S_IFMT;
        dir->st.st_mode |=  S_IFMT;

    });

    return 0;
}