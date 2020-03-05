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


typedef struct {
    list(inode_t*, children);
} tmpfs_t;

typedef struct {

    struct stat st;

    size_t capacity;
    void* data;

} tmpfs_inode_t;




int tmpfs_getattr (inode_t*, struct stat*);
int tmpfs_setattr (inode_t*, struct stat*);

int tmpfs_truncate (inode_t*, off_t);

ssize_t tmpfs_read (inode_t*, void*, off_t, size_t);
ssize_t tmpfs_write (inode_t*, const void*, off_t, size_t);
ssize_t tmpfs_readlink (inode_t*, char*, size_t);

inode_t* tmpfs_creat (inode_t*, const char*, mode_t);
inode_t* tmpfs_finddir (inode_t*, const char*);
ssize_t tmpfs_readdir (inode_t*, struct dirent*, off_t, size_t);

int tmpfs_rename (inode_t*, const char*, const char*);
int tmpfs_symlink (inode_t*, const char*, const char*);
int tmpfs_unlink (inode_t*, const char*);


void* tmpfs_cache_load (vfs_cache_t*, ino_t);
void tmpfs_cache_flush (vfs_cache_t*, ino_t, void*);

#endif