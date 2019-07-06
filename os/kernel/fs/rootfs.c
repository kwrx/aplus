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


inode_t _vfs_root;
inode_t* vfs_root = &_vfs_root;



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

    memset(&_vfs_root, 0, sizeof(_vfs_root));


    _vfs_root.name[0] = '/';
    _vfs_root.name[1] = '\0';
    _vfs_root.ino = 1;
    _vfs_root.ops.getattr = rootfs_getattr;

    spinlock_init(&_vfs_root.lock);

}