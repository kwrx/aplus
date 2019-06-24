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
int tmpfs_unlink(inode_t* inode, const char* name) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);


    inode_t* d = NULL;

    list_each(TMPFS(inode)->children, i) {
        if(likely(i->parent != inode))
            continue;

        if(likely(strcmp(i->name, name) != 0))
            continue;

        d = i;
        break;
    }

    if(!d)
        return errno = ENOENT, -1;


    d->mount.st.f_bavail += d->st.st_size;
    d->mount.st.f_bfree += d->st.st_size;
    d->mount.st.f_ffree++;
    d->mount.st.f_favail++;

    list_remove(TMPFS(inode)->children, d);

    kfree(d->userdata);
    kfree(d);

    return 0;
}