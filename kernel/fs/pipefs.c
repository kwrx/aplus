/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2022 Antonino Natale
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

#include <aplus/utils/ringbuffer.h>



#define PIPEFS_FSID         0xDEADCAFE
#define PIPEFS_FIRST_INO    0xFFFFFFFFF000000



static struct superblock pipefs_superblock = {
    .fsid   = PIPEFS_FSID,
    .flags  = ST_NOATIME | ST_NODEV | ST_NOSUID | ST_NOEXEC | ST_SYNCHRONOUS,
    .dev    = NULL,
    .root   = NULL,
    .ino    = PIPEFS_FIRST_INO,
    .fsinfo = NULL,
    .st     = {
        .f_bsize   = CONFIG_BUFSIZ,
        .f_frsize  = CONFIG_BUFSIZ,
        .f_blocks  = 0,
        .f_bfree   = 0,
        .f_bavail  = 0,
        .f_files   = 0,
        .f_ffree   = 0,
        .f_favail  = 0,
        .f_fsid    = PIPEFS_FSID,
        .f_flag    = 0,
        .f_namemax = 0
    },
};

static ino64_t __pipefs_next_ino = PIPEFS_FIRST_INO + 1;



int pipefs_close(inode_t* inode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    
    ringbuffer_destroy((ringbuffer_t*) inode->userdata);
    return 0;

}


ssize_t pipefs_read(inode_t* inode, void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(buf);

    if(unlikely(size == 0))
        return 0;


    ringbuffer_t* rb = (ringbuffer_t*) inode->userdata;

    return ringbuffer_read(rb, buf, size);

}


ssize_t pipefs_write(inode_t* inode, const void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(buf);

    if(unlikely(size == 0))
        return 0;


    ringbuffer_t* rb = (ringbuffer_t*) inode->userdata;

    return ringbuffer_write(rb, buf, size);

}




inode_t* vfs_mkfifo(size_t bufsize, int flags) {

    DEBUG_ASSERT(bufsize);

    ringbuffer_t* rb = kcalloc(sizeof(ringbuffer_t), 1, sizeof(ringbuffer_t));
   
    if(unlikely(!rb))
        return errno = ENOMEM, NULL;


    inode_t* inode = kcalloc(sizeof(inode_t), 1, GFP_KERNEL);

    if(unlikely(!inode))
        return errno = ENOMEM, NULL;


    ringbuffer_init(rb, bufsize);

    inode->name[0]      = '\0';
    inode->ino          = __pipefs_next_ino++;
    inode->sb           = &pipefs_superblock;
    inode->parent       = NULL;
    inode->userdata     = rb;
    inode->ops.open     = NULL;
    inode->ops.close    = pipefs_close;
    inode->ops.read     = pipefs_read;
    inode->ops.write    = pipefs_write;

    spinlock_init(&inode->lock);


    return inode;

}