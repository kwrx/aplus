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


#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/blkdev.h>
#include <libc.h>


typedef struct {
    uint8_t active;
    uint8_t head;
    uint16_t sector;
    uint8_t sysid;
    uint8_t ehead;
    uint16_t esector;
    uint32_t lba;
    uint32_t size;
} mbr_ptable_t ;

typedef struct {
    inode_t* dev;
    off64_t offset;
    off64_t size;
} mbr_partition_t;




static int mbr_read(inode_t* ino, void* buf, off_t pos, size_t size) {
    if(unlikely(!ino)) {
        errno = EINVAL;
        return 0;
    }

    mbr_partition_t* mbr = (mbr_partition_t*) ino->userdata;
    if(unlikely(!mbr)) {
        errno = EINVAL;
        return 0;
    }

    return vfs_read(mbr->dev, buf, pos + mbr->offset, size);
}

static int mbr_write(inode_t* ino, void* buf, off_t pos, size_t size) {
    if(unlikely(!ino)) {
        errno = EINVAL;
        return 0;
    }

    mbr_partition_t* mbr = (mbr_partition_t*) ino->userdata;
    if(unlikely(!mbr)) {
        errno = EINVAL;
        return 0;
    }

    return vfs_write(mbr->dev, buf, pos + mbr->offset, size);
}

int blkdev_init_mbr(blkdev_t* blkdev) {
    struct inode* dev = blkdev->dev;
    
    if(unlikely(!dev))
        return -1;

    mbr_ptable_t table[4];
    char name[2];


    if(unlikely(vfs_read(dev, &table, 0x1BE, 64) != 64))
        return -1;

    
    int i, j = 0;
    for(i = 0; i < 4; i++) {
        if(table[i].sysid == 0)
            continue;

        mbr_partition_t* mbr = (mbr_partition_t*) kmalloc(sizeof(mbr_partition_t), GFP_KERNEL);
        if(unlikely(!mbr))
            return -1;

        mbr->dev = dev;
        mbr->offset = (off64_t) table[i].lba * 512;
        mbr->size = (off64_t) table[i].size * 512;

        name[0] = '0' + j++;
        name[1] = '\0';

        inode_t* ch = vfs_mkdev(dev->name, i, S_IFBLK | blkdev->mode);
        if(unlikely(!ch))
            return -1;

        ch->size = (off64_t) table[i].size * 512;
        ch->userdata = (void*) mbr;

        
        ch->ioctl = dev->ioctl;
        ch->open = dev->open;
        ch->close = dev->close;
        
        ch->read = mbr_read;
        ch->write = mbr_write;
    }


    return 0;
}