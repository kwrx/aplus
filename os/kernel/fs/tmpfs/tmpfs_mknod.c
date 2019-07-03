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
inode_t* tmpfs_mknod(inode_t* inode, const char * name, mode_t mode) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->ino);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsinfo);
    DEBUG_ASSERT(inode->sb->fsid == TMPFS_ID);
    DEBUG_ASSERT(name);


    static ino_t next_ino = 1;
    

    inode_t* i = (inode_t*) kcalloc(1, sizeof(inode_t), GFP_USER);

    i->parent = inode;
    i->sb = PTR_REF(inode->sb);

    i->ino = PTR_REF(&i->__ino);
    i->ino->st.st_ino = ++next_ino;
    i->ino->st.st_mode = mode & ~current_task->umask;
    i->ino->st.st_nlink = 1;

    strncpy(i->name, name, sizeof(i->name));



    if(S_ISDIR(mode)) {

        i->ino->ops.finddir = tmpfs_finddir;
        i->ino->ops.readdir = tmpfs_readdir;
        i->ino->ops.unlink = tmpfs_unlink;
        i->ino->ops.mknod = tmpfs_mknod;

    }

    if(S_ISREG(mode)) {

        i->ino->ops.read = tmpfs_read;     
        i->ino->ops.write = tmpfs_write;     

    }

    spinlock_init(&i->ino->lock);


    i->sb->st.f_ffree--;
    i->sb->st.f_favail--;


    tmpfs_t* tmpfs = (tmpfs_t*) inode->fsinfo;
    list_push(tmpfs->children, i);

    return i;
}