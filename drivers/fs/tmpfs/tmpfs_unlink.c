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

#include "tmpfs.h"


int tmpfs_unlink(struct inode* inode, char* name) {
    if(inode->childs) {
        struct inode_childs* tmp;
        for(tmp = inode->childs; tmp; tmp = tmp->next) {
            if(strcmp(tmp->inode->name, name) == 0) {
                kfree((void*) tmp->inode->userdata);
                
                inode->mtinfo->stat.f_bfree += inode->size;
                inode->mtinfo->stat.f_bavail += inode->size;
                inode->mtinfo->stat.f_ffree++;
                inode->mtinfo->stat.f_favail++;


                return 0;
            }
        }
    }

    return -1;
}
