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
#include <errno.h>

#include "tmpfs.h"


__thread_safe
ssize_t tmpfs_write(inode_t* inode, const void* buf, off_t pos, size_t len) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == TMPFS_ID);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    tmpfs_inode_t* i = (tmpfs_inode_t*) vfs_cache_get(&inode->sb->cache, inode->ino);


    if(pos + len > i->st.st_size) {
        
        if(unlikely(
            (
                (long) inode->sb->st.f_bavail - (long) ((pos + len) - i->st.st_size)
            ) <= 0L)
            
        )
            return errno = ENOSPC, -1;


        if(pos + len > i->capacity) {

            i->capacity = BUFSIZ + pos + len;
            i->data = krealloc(i->data, i->capacity, GFP_USER);
        
        }

        inode->sb->st.f_bfree -= (pos + len) - i->st.st_size;
        inode->sb->st.f_bavail -= (pos + len) - i->st.st_size;
        i->st.st_size = pos + len;
    }
    
    
    memcpy((void*) ((uintptr_t) i->data + (uintptr_t) pos), buf, len);
    return len;
}