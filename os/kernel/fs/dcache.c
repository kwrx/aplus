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
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <string.h>
#include <errno.h>

#include <aplus/utils/list.h>


static list(inode_t*, dcache);


void dcache_init(void) {
    memset(&dcache, 0, sizeof(dcache));
}


void vfs_dcache_add(inode_t* inode) {

    DEBUG_ASSERT(inode);
    
    list_push(dcache, inode);
}


void vfs_dcache_remove(inode_t* inode) {
    
    DEBUG_ASSERT(inode);

    list_remove(dcache, inode);
    kfree(inode);
}


inode_t* vfs_dcache_find(inode_t* parent, const char* name) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(name);

    list_each(dcache, e) {

        if(likely(e->parent != parent))
            continue;

        if(strcmp(e->name, name) != 0)
            continue;

        return e;
    }

    return NULL;
}