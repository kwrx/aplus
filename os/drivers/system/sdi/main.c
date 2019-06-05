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
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/utils/list.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>

#include <sdi/device.h>
#include <sdi/chrdev.h>


MODULE_NAME("sdi");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


list(inode_t*, devices);


static inode_t* devfs_finddir(inode_t* inode, char __user * name) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(inode == vfs_dev);
    
    if(unlikely(!ptr_check(name, R_OK)))
        return -EFAULT;

    list_each(devices, d)
        if(strcmp(d->name, name) == 0)
            return d;

    return -ENOENT;
}

static int devfs_getdents(inode_t* inode, struct dirent __user * e, off_t position, size_t size) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(e);
    DEBUG_ASSERT(inode == vfs_dev);
    
    if(unlikely(!ptr_check(e, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(position > list_length(devices)))
        return 0;


    int s = size / sizeof(struct dirent);
    int i;

    for(i = 0; i < s; i++) {

        inode_t* d = list_get_at(devices, position);

        if(!d)
            break;

        e[i].d_ino = d->st.st_ino;
        e[i].d_off = position;
        e[i].d_reclen = sizeof(struct dirent);
        e[i].d_type = d->st.st_mode;
        strncpy(e[i].d_name, d->name, MAXNAMLEN);

        position++;
    }

    return i * sizeof(struct dirent);
}



void init(const char* args) {
    DEBUG_ASSERT(&devices);
    
    memset(&devices, 0, sizeof(devices));


    inode_t* d = (inode_t*) kcalloc(1, sizeof(inode_t), GFP_KERNEL);
    DEBUG_ASSERT(d);

    d->name[0] = 'd';
    d->name[1] = 'e';
    d->name[2] = 'v';
    d->name[3] = '\0';

    d->st.st_ino = 1;
    d->st.st_mode = S_IFDIR;
    d->st.st_nlink = 1;

    d->ops.finddir = devfs_finddir;
    d->ops.getdents = devfs_getdents;

    spinlock_init(&d->lock);

    vfs_dev = d;

}

void dnit(void) {
    
}