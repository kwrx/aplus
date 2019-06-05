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



inode_t* tmpfs_mknod(inode_t* inode, const char __user * name, mode_t mode) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if(unlikely(!ptr_check(name, R_OK)))
        return -EFAULT;

    static ino_t next_ino = 1;
    

    inode_t* i = (inode_t*) kcalloc(1, sizeof(inode_t), GFP_USER);

    i->parent = inode;
    i->st.st_ino = ++next_ino;
    i->st.st_mode = mode & ~current_task->umask;
    i->st.st_nlink = 1;

    strncpy(i->name, name, sizeof(i->name));


    
    if((inode->st.st_mode & ~S_IFMT) == S_IFMT)
        i->root = inode;
    else
        i->root = inode->root;

    DEBUG_ASSERT(i->root);


    if(S_ISDIR(mode)) {

        i->ops.finddir = tmpfs_finddir;
        i->ops.getdents = tmpfs_getdents;
        i->ops.unlink = tmpfs_unlink;
        i->ops.mknod = tmpfs_mknod;

    }

    if(S_ISREG(mode)) {

        i->ops.read = tmpfs_read;     
        i->ops.write = tmpfs_write;     

    }

    spinlock_init(&i->lock);



    i->root->mount.st.f_ffree--;
    i->root->mount.st.f_favail--;

    list_push(TMPFS(i)->children, i);

    return i;
}