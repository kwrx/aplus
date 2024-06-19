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

#include <aplus/utils/list.h>

#include "tmpfs.h"



int tmpfs_rename (inode_t* inode, const char* name, const char* newname) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);

    DEBUG_ASSERT(name);
    DEBUG_ASSERT(newname);



    tmpfs_t* tmpfs = (tmpfs_t*) inode->sb->fsinfo;
    inode_t* d = NULL;


    list_each(tmpfs->children, i) {

        if(likely(i->parent != inode))
            continue;

        if(likely(strcmp(i->name, name) != 0))
            continue;

        d = i;
        break;

    }

    if(!d) {
        return errno = ENOENT, -1;
    }
   
    strncpy(d->name, newname, CONFIG_MAXNAMLEN);


    return 0;
    
}