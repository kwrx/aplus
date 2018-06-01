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


#ifndef _FAT_H
#define _FAT_H

#include "fatlib/fat_opts.h"
#include "fatlib/fat_defs.h"
#include "fatlib/fat_types.h"
#include "fatlib/fat_cache.h"
#include "fatlib/fat_misc.h"
#include "fatlib/fat_string.h"
#include "fatlib/fat_table.h"
#include "fatlib/fat_write.h"
#include "fatlib/fat_access.h"


typedef struct {
    struct fatfs* sb;
    struct fatfs_cache cache;
    uint32_t cluster;
} fat_t;


int fat_mount(struct inode* dev, struct inode* dir, struct mountinfo* info);
int fat_open(struct inode* inode);

struct inode* fat_mknod(struct inode* inode, char* name, mode_t mode);
struct inode* fat_finddir(struct inode* inode, char* name);
int fat_unlink(struct inode* inode, char* name);

int fat_write(struct inode* inode, void* ptr, off_t pos, size_t len);
int fat_read(struct inode* inode, void* ptr, off_t pos, size_t len);

int fat_fsync(struct inode* inode);


inode_t* fat_mkchild(inode_t* parent, struct fat_dir_entry* e, char* name, mode_t mode);
char* fat_getname(struct fat_dir_entry* e, char* lfn);

#endif
