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
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/timer.h>
#include <libc.h>

#include "tmpfs.h"


struct inode* tmpfs_mknod(struct inode* inode, char* name, mode_t mode) {
    inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_KERNEL);
    memset(child, 0, sizeof(inode_t));

    child->name = strdup(name);
    child->ino = vfs_inode();

    child->mode = mode & ~current_task->umask;

    child->dev =
    child->rdev =
    child->nlink = 0;
    child->uid = current_task->uid;
    child->gid = current_task->gid;
    child->size = 0;

    child->atime = 
    child->ctime = 
    child->mtime = timer_gettimestamp();
    
    child->parent = inode;
    child->mtinfo = inode->mtinfo;
    child->link = NULL;

    child->childs = NULL;

    if(S_ISDIR(mode)) {
        child->mknod = tmpfs_mknod;
        child->unlink = tmpfs_unlink;
    } else {
        child->read = tmpfs_read;
        child->write = tmpfs_write;
    }


    child->finddir = NULL;
    child->rename = NULL;
    child->chown = NULL;
    child->chmod = NULL;
    child->ioctl = NULL;
    child->open = NULL;
    child->close = NULL;

    inode->mtinfo->stat.f_ffree--;
    inode->mtinfo->stat.f_favail--;

    return child;
}
