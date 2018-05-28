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


int procfs_devices_init(procfs_entry_t* e) {
    return 0;
}


int procfs_devices_update(procfs_entry_t* e) {

    memset(e->data, 0, strlen(e->data));
    strcat(e->data, "Character devices:\n");

    struct inode_childs* cx;
    for(cx = devfs->childs; cx; cx = cx->next) {
        if(!S_ISCHR(cx->inode->mode))
            continue;

        char buf[64];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "%3d %s\n", cx->inode->dev, cx->inode->name);
        strcat(e->data, buf);
    }


    strcat(e->data, "\nBlock devices:\n");

    for(cx = devfs->childs; cx; cx = cx->next) {
        if(!S_ISBLK(cx->inode->mode))
            continue;

        char buf[64];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "%3d %s\n", cx->inode->dev, cx->inode->name);
        strcat(e->data, buf);
    }

    e->size = strlen(e->data);
    return 0;
}