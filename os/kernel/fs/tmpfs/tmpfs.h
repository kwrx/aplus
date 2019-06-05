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


#ifndef _TMPFS_H
#define _TMPFS_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>

#include <aplus/utils/list.h>


#define TMPFS_SIZE_MAX          (4 * 1024 * 1024)
#define TMPFS_NODES_MAX         (4096)

#define TMPFS(i)                            \
    ((tmpfs_t*)                             \
    (((i->st.st_mode & ~S_IFMT) == S_IFMT)  \
        ? i->mount.userdata                 \
        : i->root->mount.userdata)          \
    )

typedef struct {
    list(inode_t*, children);
} tmpfs_t;


inode_t* tmpfs_finddir (inode_t*, const char*);
inode_t* tmpfs_mknod (inode_t*, const char* name, mode_t mode);
int tmpfs_getdents (inode_t*, struct dirent*, off_t, size_t);
int tmpfs_unlink (inode_t*, const char* name);
int tmpfs_read(inode_t*, void __user *, off_t, size_t);
int tmpfs_write(inode_t*, const void __user *, off_t, size_t);

#endif