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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "tmpfs.h"



/**
 * @brief Creates an inode in a tmpfs directory.
 *
 * @param inode The directory.
 * @param name The name of the new entry.
 * @param mode The type and permissions of the new entry.
 * @return The new inode, or NULL with errno set.
 */
inode_t* tmpfs_creat(inode_t* inode, const char* name, mode_t mode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_TMPFS);
    DEBUG_ASSERT(name);


    tmpfs_t* tmpfs = (tmpfs_t*)inode->sb->fsinfo;
    inode_t* d     = (inode_t*)kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!d))
        return errno = ENOMEM, NULL;


    mode_t umask = 0;

    shared_ptr_access(current_task->fs, fs, { umask = fs->umask; });


    strncpy(d->name, name, CONFIG_MAXNAMLEN);

    d->sb     = inode->sb;
    d->parent = inode;

    spinlock_init(&d->lock);



    d->ops.getattr = tmpfs_getattr;
    d->ops.setattr = tmpfs_setattr;
    d->ops.release = tmpfs_release;


    if (S_ISDIR(mode)) {

        d->ops.creat   = tmpfs_creat;
        d->ops.finddir = tmpfs_finddir;
        d->ops.readdir = tmpfs_readdir;
        d->ops.rename  = tmpfs_rename;
        d->ops.symlink = tmpfs_symlink;
        d->ops.unlink  = tmpfs_unlink;

        vfs_dcache_init(d);
    }


    if (S_ISREG(mode)) {

        d->ops.truncate = tmpfs_truncate;
        d->ops.read     = tmpfs_read;
        d->ops.write    = tmpfs_write;
    }


    if (S_ISLNK(mode)) {

        d->ops.readlink = tmpfs_readlink;
    }


    if (S_ISFIFO(mode)) {

        d->ops.open = fifofs_open;
    }



    int e = 0;

    scoped_lock(&tmpfs->lock) {

        if (inode != inode->sb->root && ((tmpfs_inode_t*)cache_get(&inode->sb->cache, inode->ino))->st.st_nlink == 0) {
            e = ENOENT;
            break;
        }

        if (unlikely(inode->sb->st.f_ffree == 0)) {
            e = ENOSPC;
            break;
        }

        inode->sb->st.f_ffree--;
        inode->sb->st.f_favail--;


        tmpfs_inode_t* i = (tmpfs_inode_t*)cache_get(&inode->sb->cache, ++tmpfs->next_ino);

        i->capacity   = 0;
        i->data       = NULL;
        i->st.st_mode = mode & ~umask;

        d->ino = i->st.st_ino;

        list_push(tmpfs->children, d);
    }


    if (unlikely(e)) {

        vfs_dcache_free(d);
        kfree(d);

        return errno = e, NULL;
    }

    return d;
}
