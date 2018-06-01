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
#include <libc.h>

#include "procfs.h"


int procfs_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {
    (void) dev;


    dir->open = procfs_open;
    dir->close = NULL;
    dir->finddir = procfs_finddir;
    dir->mknod = NULL;
    dir->unlink = NULL;
    dir->read = NULL;
    dir->write = NULL;
    dir->ioctl = NULL;
    dir->mtinfo = info;

    info->stat.f_bsize = PAGE_SIZE;
    info->stat.f_frsize = PAGE_SIZE;
    info->stat.f_blocks = 0;
    info->stat.f_bfree = 0;
    info->stat.f_bavail = 0;
    info->stat.f_files = 0;
    info->stat.f_ffree = 0;
    info->stat.f_favail = 0;
    info->stat.f_namemax = UINT32_MAX;


    procfs_entry_t* sb = (procfs_entry_t*) kmalloc(sizeof(procfs_entry_t), GFP_KERNEL);
    if(unlikely(!sb)) {
        kprintf(ERROR "procfs: no memory left!\n");
        return -1;
    }

    memset(sb, 0, sizeof(sb));
    strcpy(sb->name, "proc");
    sb->task = NULL;
    sb->mode = S_IFDIR;
    sb->parent = NULL;
    sb->init = procfs_init;
    sb->update = procfs_update;
    dir->userdata = (void*) sb;
    

    sb->init(sb);
    return 0;
}
