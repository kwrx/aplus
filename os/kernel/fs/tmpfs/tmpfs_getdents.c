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

#include <aplus/utils/list.h>

#include "tmpfs.h"



__thread_safe
int tmpfs_getdents(inode_t* inode, struct dirent* e, off_t pos, size_t len) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(e);

    
    
    int s = len / sizeof(struct dirent);
    int i;

    for(i = 0; i < s; i++) {

        int p = pos;

        list_each(TMPFS(inode)->children, d) {

            if(p--)
                continue;

            e[i].d_ino = d->st.st_ino;
            e[i].d_off = pos;
            e[i].d_reclen = sizeof(struct dirent);
            e[i].d_type = d->st.st_mode;
            strncpy(e[i].d_name, d->name, MAXNAMLEN);

            p = 1;
            break;
        }

        if(p == 0)
            break;

        pos++;
    }

    return i * sizeof(struct dirent);
}