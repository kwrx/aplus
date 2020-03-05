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
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <stdint.h>


#include "tmpfs.h"




inode_t* tmpfs_creat(inode_t* inode, const char * name, mode_t mode) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == TMPFS_ID);
    DEBUG_ASSERT(name);
    

    static ino_t next_ino = 1;


    if(inode->sb->st.f_ffree == 0)
        return errno = ENOSPC, NULL;

    

    tmpfs_inode_t* i = (tmpfs_inode_t*) vfs_cache_get(&inode->sb->cache, ++next_ino);

    i->capacity = 0;
    i->data = NULL;
    i->st.st_mode = mode & ~current_task->umask;
    


    inode_t* d = (inode_t*) kcalloc(sizeof(inode_t), 1, GFP_KERNEL);

    strncpy(d->name, name, MAXNAMLEN);

    d->ino = i->st.st_ino;
    d->sb = inode->sb;
    d->parent = inode;

    spinlock_init(&d->lock);



    d->ops.getattr = tmpfs_getattr;
    d->ops.setattr = tmpfs_setattr;


    if(S_ISDIR(mode)) {

        d->ops.creat   = tmpfs_creat;
        d->ops.finddir = tmpfs_finddir;
        d->ops.readdir = tmpfs_readdir;
        d->ops.rename  = tmpfs_rename;
        d->ops.symlink = tmpfs_symlink;
        d->ops.unlink  = tmpfs_unlink;
    
    }


    if(S_ISREG(mode)) {

        d->ops.truncate = tmpfs_truncate;
        d->ops.read     = tmpfs_read; 
        d->ops.write    = tmpfs_write; 

    }


    if(S_ISLNK(mode)) {

        d->ops.readlink = tmpfs_readlink;

    }



    inode->sb->st.f_ffree--;
    inode->sb->st.f_favail--;


    tmpfs_t* tmpfs = (tmpfs_t*) inode->sb->fsinfo;
    list_push(tmpfs->children, d);

    return d;
}