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
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "ext2.h"


int ext2_symlink_open(struct inode* inode) {
    if(unlikely(!inode || !inode->parent)) {
        errno = EINVAL;
        return -1;
    }

    ext2_priv_t* priv = (ext2_priv_t*) inode->userdata;
    if(unlikely(!priv)) {
        errno = EINVAL;
        return -1;
    }


    char buf[BUFSIZ];
    if(inode->size >= 60)
        vfs_read(inode, buf, 0, sizeof(buf));
    else
        strncpy(buf, (const char*) &priv->inode.dbp, 60);

    
    inode_t* cwd = current_task->cwd;
    current_task->cwd = inode->parent;

    int fd = sys_open(buf, O_RDONLY, 0);
    if(fd < 0)
        goto done;

    inode->link = current_task->fd[fd].inode;
    sys_close(fd);

done:
    current_task->cwd = cwd;
    return 0;
}