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
int tmpfs_write(inode_t* inode, const void* buf, off_t pos, size_t len) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    if(pos + len > inode->st.st_size) {
        
        DEBUG_ASSERT(inode->root);

        if(unlikely(
            (
                (long) inode->root->mount.st.f_bavail - (long) ((pos + len) - inode->st.st_size)
            ) <= 0L)
            
        )
            return errno = ENOSPC, -1;


        inode->userdata = krealloc(inode->userdata, pos + len, GFP_USER);
        inode->root->mount.st.f_bfree -= (pos + len) - inode->st.st_size;
        inode->root->mount.st.f_bavail -= (pos + len) - inode->st.st_size;
        inode->st.st_size = pos + len;
    }
    
    
    memcpy((void*) ((uintptr_t) inode->userdata + (uintptr_t) pos), buf, len);
    return len;
}