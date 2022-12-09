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


static inode_t** __next_entry(list(inode_t*, children), inode_t* parent, inode_t** curr) {

    if(curr == NULL) {

        curr = list_elem_front(children);

    } else {

        curr = list_elem_next(curr);

    }

    while(curr && (*curr)->parent != parent) {

        curr = list_elem_next(curr);

        if(curr == NULL)
            return NULL;

    }

    return curr;

}


ssize_t tmpfs_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsinfo);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);
    
    DEBUG_ASSERT(e);

    if(unlikely(count == 0))
        return 0;
 

    tmpfs_t*  tmpfs = (tmpfs_t*) inode->sb->fsinfo;
    inode_t** entry = NULL;


    if(pos > 1) {

        for(off_t i = 1; i < pos; i++) {

            entry = __next_entry(tmpfs->children, inode, entry);

            if(entry == NULL)
                return 0;

        }

    }


    off_t i = 0;

    for(off_t j = pos; j < pos + count; j++, i++) {

        switch(j) {

            case 0: {

                struct stat st = { 0 };
                
                if(inode->ops.getattr) {
                    inode->ops.getattr(inode, &st);
                }

                e[i].d_ino    = inode->ino;
                e[i].d_off    = i;
                e[i].d_reclen = sizeof(struct dirent);
                e[i].d_type   = MODE_2_DIRENT_TYPE(st.st_mode);

                strncpy(e[i].d_name, ".", sizeof(e[i].d_name));

                break;

            }

            case 1: {    
                
                DEBUG_ASSERT(inode->parent);

                struct stat st = { 0 };

                if(inode->parent->ops.getattr) {
                    inode->parent->ops.getattr(inode->parent, &st);
                }

                e[i].d_ino    = inode->parent->ino;
                e[i].d_off    = i;
                e[i].d_reclen = sizeof(struct dirent);
                e[i].d_type   = MODE_2_DIRENT_TYPE(st.st_mode);

                strncpy(e[i].d_name, "..", sizeof(e[i].d_name));

                break;

            }

            default: {

                if(unlikely(!entry))
                    return i;


                tmpfs_inode_t* c = cache_get(&inode->sb->cache, (*entry)->ino);

                e[i].d_ino    = c->st.st_ino;
                e[i].d_off    = i;
                e[i].d_reclen = sizeof(struct dirent);
                e[i].d_type   = MODE_2_DIRENT_TYPE(c->st.st_mode);

                strncpy(e[i].d_name, (*entry)->name, sizeof(e[i].d_name));


                entry = __next_entry(tmpfs->children, inode, entry);

                break;

            }


        }

    }

    return i;
    
}