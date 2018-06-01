/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"

struct inode* procfs_finddir(struct inode* inode, char* name) {
    if(unlikely(!inode || !inode->userdata)) {
        errno = EINVAL;
        return NULL;
    }

    procfs_entry_t* e = (procfs_entry_t*) inode->userdata;
    if(e->update)
        e->update(e);

    
    list_each(e->childs, v) {
        if(strcmp(name, v->name) != 0)
            continue;

        if(v->update)
            v->update(v);



        inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_KERNEL);
        memset(child, 0, sizeof(inode_t));


        child->name = strdup(v->name);
        child->ino = vfs_inode();

        child->mode = (v->mode | 0666) & ~kernel_task->umask;

        child->dev =
        child->rdev =
        child->nlink = 0;
        child->uid = kernel_task->uid;
        child->gid = kernel_task->gid;
        child->size = v->size;

        child->atime = 
        child->ctime = 
        child->mtime = timer_gettimestamp();
        
        child->parent = inode;
        child->link = (inode_t*) (S_ISLNK(v->mode) ? v->link : NULL);

        child->childs = NULL;
        child->userdata = v;
        child->mtinfo = inode->mtinfo;

        if(!S_ISDIR(v->mode))
            child->read = procfs_read;
        
        child->finddir = procfs_finddir;
        child->chown = NULL;
        child->chmod = NULL;
        child->ioctl = NULL;
        child->close = NULL;



        struct inode_childs* cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
        cx->inode = child;
        cx->next = inode->childs;
        inode->childs = cx;

        return child;
    }

    errno = ENOENT;
    return NULL;
}