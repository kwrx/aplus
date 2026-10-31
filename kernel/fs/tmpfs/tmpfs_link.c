/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include <aplus/utils/list.h>

#include "tmpfs.h"



int tmpfs_link(inode_t* inode, inode_t* target, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);

    DEBUG_ASSERT(target);
    DEBUG_ASSERT(name);


    if (unlikely(target->sb != inode->sb))
        return errno = EXDEV, -1;

    if (unlikely(target == inode->sb->root))
        return errno = EPERM, -1;


    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    inode_t* d     = (inode_t*)kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!d))
        return errno = ENOMEM, -1;


    strncpy(d->name, name, CONFIG_MAXNAMLEN - 1);

    d->ino    = target->ino;
    d->sb     = inode->sb;
    d->parent = inode;
    d->ops    = target->ops;

    spinlock_init(&d->lock);



    int e = 0;

    scoped_lock(&tmpfs->lock) {

        if (inode != inode->sb->root && ((tmpfs_inode_t*)cache_get(&inode->sb->cache, inode->ino))->st.st_nlink == 0) {
            e = ENOENT;
            break;
        }

        list_each(tmpfs->children, i) {

            if (i->parent == inode && strcmp(i->name, name) == 0) {
                e = EEXIST;
                break;
            }
        }

        if (e)
            break;


        tmpfs_inode_t* ti = (tmpfs_inode_t*)cache_get(&inode->sb->cache, target->ino);

        if (!S_ISREG(ti->st.st_mode) && !S_ISLNK(ti->st.st_mode)) {
            e = EPERM;
            break;
        }

        if (ti->st.st_nlink == 0) {
            e = ENOENT;
            break;
        }

        ti->st.st_nlink++;
        ti->st.st_ctime = arch_timer_gettime();
        ti->inodes++;

        list_push(tmpfs->children, d);
    }


    if (unlikely(e)) {

        kfree(d);

        return errno = e, -1;
    }

    return 0;
}
