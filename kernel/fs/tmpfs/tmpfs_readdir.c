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
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <stdint.h>

#include <aplus/utils/list.h>

#include "tmpfs.h"




ssize_t tmpfs_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == TMPFS_ID);
    
    DEBUG_ASSERT(e);

 

    tmpfs_t* tmpfs = (tmpfs_t*) inode->sb->fsinfo;


    
    int i;
    for(i = 0; i < count; i++) {

        int p = pos;

        list_each(tmpfs->children, d) {

            if(likely(d->parent != inode))
                continue;
            
            if(p--)
                continue;


            tmpfs_inode_t* c = (tmpfs_inode_t*) vfs_cache_get(&inode->sb->cache, d->ino);

            e[i].d_ino = c->st.st_ino;
            e[i].d_off = pos;
            e[i].d_reclen = sizeof(struct dirent);
            e[i].d_type = c->st.st_mode >> 12; /* FIXME: find a better way */
            strncpy(e[i].d_name, d->name, CONFIG_MAXNAMLEN);

            p = 1;
            break;
        }

        if(p == 0)
            break;

        pos++;
    }

    return i;
}