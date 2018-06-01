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
#include <aplus/sysconfig.h>
#include <libc.h>

#include "tmpfs.h"

int tmpfs_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {
    (void) dev;


    dir->open = NULL;
    dir->close = NULL;
    dir->unlink = tmpfs_unlink;
    dir->mknod = tmpfs_mknod;
    dir->finddir = NULL;
    dir->mtinfo = info;


    uint32_t size = (uint32_t) sysconfig("fs.tmpfs.size", 0x1000000);
    uint32_t files = (uint32_t) sysconfig("fs.tmpfs.files", 0x100000);
 
    size = size ? size : 0x1000000;
    files = files ? files : 0x100000;



    info->stat.f_bsize = 1;
    info->stat.f_frsize = 1;
    info->stat.f_blocks =
    info->stat.f_bfree =
    info->stat.f_bavail = size;
    info->stat.f_files =
    info->stat.f_ffree =
    info->stat.f_favail = files;
    info->stat.f_namemax = UINT32_MAX;


    return 0;
}
