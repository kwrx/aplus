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


#ifndef _BLKDEV_H
#define _BLKDEV_H

#include <aplus.h>
#include <aplus/base.h>

#define BLKDEV_BLKMAXSIZE           0x1000

#define BLKDEV_FLAGS_RDONLY         1
#define BLKDEV_FLAGS_MBR            2



typedef struct {
    inode_t* dev;
    mode_t mode;
    size_t blksize;
    size_t blkcount;

    size_t (*read) (void* userdata, uint32_t blkno, void* buf, size_t count);
    size_t (*write) (void* userdata, uint32_t blkno, void* buf, size_t count);

    struct {
        char c_data[BLKDEV_BLKMAXSIZE];
        uint32_t c_blkno;
        int c_cached;
    } cache;

    void* userdata;
} blkdev_t;


int blkdev_register_device(blkdev_t* blk, char* name, int idx, int flags);
int blkdev_unregister_device(char* name);
int blkdev_read(inode_t* ino, void* buf, off_t pos, size_t size);
int blkdev_write(inode_t* inode, void* buf, off_t pos, size_t size);


#endif