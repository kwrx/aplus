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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>

#include "tmpfs.h"



int tmpfs_setattr(inode_t* inode, struct stat* st) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == TMPFS_ID);

    DEBUG_ASSERT(st);


    tmpfs_inode_t* i = (tmpfs_inode_t*) vfs_cache_get(&inode->sb->cache, inode->ino);
   
    i->st.st_mode = st->st_mode;
    i->st.st_uid = st->st_uid;
    i->st.st_gid = st->st_gid;

    memcpy(&i->st.st_atim, &st->st_atim, sizeof(st->st_atim));
    memcpy(&i->st.st_mtim, &st->st_mtim, sizeof(st->st_mtim));
    memcpy(&i->st.st_ctim, &st->st_ctim, sizeof(st->st_ctim));
    
    return 0;
}