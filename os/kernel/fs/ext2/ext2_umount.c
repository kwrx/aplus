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

#include "ext2.h"



__thread_safe
int ext2_umount(inode_t* dir) {

    DEBUG_ASSERT(dir);


    struct vfs_sb* sb = smartptr_get(dir->sb);
    struct ientry* ino = smartptr_get(dir->ino);

    DEBUG_ASSERT(sb->fsinfo);
    DEBUG_ASSERT(sb->cache);
    DEBUG_ASSERT(sb->fsid == EXT2_ID);

    DEBUG_ASSERT((ino->st.st_mode & S_IFMT) == S_IFMT);


    __lock(&ino->lock, {

        vfs_cache_destroy(&sb->cache);


        ino->st.st_mode &= ~S_IFMT;
        ino->st.st_mode |=  S_IFDIR;

        smartptr_free_ext(dir->sb, sb, kfree(sb->fsinfo));

    });

    return 0;
}