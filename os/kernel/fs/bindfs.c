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
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <string.h>
#include <errno.h>




__thread_safe
int bindfs_mount(inode_t* dev, inode_t* dir, int flags, const char * args) {

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->sb);
    DEBUG_ASSERT(dir);

    (void) args;


    dir->sb = (struct superblock*) kcalloc(sizeof(struct superblock), 1, GFP_KERNEL);

    memcpy(dir->sb, dev->sb, sizeof(struct superblock));
    memcpy(&dir->sb->ops, &dev->ops, sizeof(struct inode_ops));
    memcpy(&dir->sb->ino, &dev->ino, sizeof(ino_t));

    return 0;
}



__thread_safe
int bindfs_umount(inode_t* dir) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dir->sb);

    return 0;
}