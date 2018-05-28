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
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>


MODULE_NAME("fs/devfs");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




static int devfs_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {
    (void) dev;

    if(!dir->parent) {
        errno = EINVAL;
        return -1;
    }



    extern inode_t* devfs;

    struct inode_childs* tmp;
    for(tmp = dir->parent->childs; tmp; tmp = tmp->next)
        if(tmp->inode == dir)
            break;

    if(tmp == NULL) {
        kprintf(ERROR "BUG: WTF! %s:%d %s()\n", __FILE__, __LINE__, __func__);

        errno = ENOENT;
        return -1;
    }

    devfs->parent = dir->parent;
    devfs->mtinfo = info;
    dir->mtinfo = info;
    tmp->inode = devfs;


    info->stat.f_bsize = PAGE_SIZE;
    info->stat.f_frsize = PAGE_SIZE;
    info->stat.f_blocks = 0;
    info->stat.f_bfree = 0;
    info->stat.f_bavail = 0;
    info->stat.f_files = 0;
    info->stat.f_ffree = 0;
    info->stat.f_favail = 0;
    info->stat.f_namemax = UINT32_MAX;

    return 0;
}


int init(void) {
    if(vfs_fsys_register(BDEVFS_MAGIC, "devtmpfs", devfs_mount) != 0)
        return -1;

    return 0;
}



int dnit(void) {
    return 0;
}
