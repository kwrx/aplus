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
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/hal.h>
#include <aplus/errno.h>



inode_t __vfs_root;
inode_t * vfs_root = &__vfs_root;



static int rootfs_getattr (inode_t* inode, struct stat* st) {
    
    st->st_dev = 0;
    st->st_ino = 1;
    st->st_mode = S_IFDIR | 0666;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_blksize = 1;
    st->st_blocks = 0;
    st->st_size = 0;

    st->st_atime = arch_timer_gettime();
    st->st_mtime = arch_timer_gettime();
    st->st_ctime = arch_timer_gettime();

    return 0;
}


void rootfs_init(void) {

    memset(&__vfs_root, 0, sizeof(__vfs_root));


    spinlock_init(&__vfs_root.lock);

    __vfs_root.name[0] = '/';
    __vfs_root.name[1] = '\0';
    __vfs_root.ino = 1;
    __vfs_root.ops.getattr = rootfs_getattr;

    vfs_dcache_init(&__vfs_root);

}