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
#include <aplus/base.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <libc.h>

#if CONFIG_IOSCHED
int iosched_blksiz = 4096;

static void iosched_rw(inode_t* inode) {
    spinlock_lock(&inode->iolock);
    
    if(list_length(inode->ioqueue) == 0)
        goto done;

        
    struct iorequest* io = list_front(inode->ioqueue);
    list_pop_front(inode->ioqueue);
    
    int e = io->fn (
        io->inode, 
        (void*) ((uintptr_t) io->buffer + io->offset),
        io->position + io->offset, 
        io->size > iosched_blksiz
            ? iosched_blksiz
            : io->size
        );
        
    if(unlikely(e <= 0)) {
        if(unlikely(e < 0))
            io->ioerr = errno;

        spinlock_unlock(&io->lock);
        goto done;
    }

    io->offset += e;

    if(e < (io->size > iosched_blksiz
            ? iosched_blksiz
            : io->size)) {

                
        spinlock_unlock(&io->lock);
        goto done;
    }


    if(io->offset < io->size)
        list_push(inode->ioqueue, io);
    else
        spinlock_unlock(&io->lock);

done:
    spinlock_unlock(&inode->iolock);
}



int iosched_read(inode_t* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!buf || !size || !inode || !inode->read)) {
        errno = EINVAL;
        return -1;
    }

    struct iorequest* io = (struct iorequest*) kmalloc(sizeof(struct iorequest), GFP_KERNEL);
    io->inode = inode;
    io->buffer = buf;
    io->size = size;
    io->position = pos;
    io->offset = 0;
    io->ioerr = 0;
    io->fn = inode->read;

    spinlock_init(&io->lock);
    spinlock_lock(&io->lock);

    list_push(inode->ioqueue, io);
    
    while(spinlock_trylock(&io->lock) != 0)
        iosched_rw(inode);


    off_t off = io->offset;
    
    if(unlikely(io->ioerr != 0)) {
        errno = io->ioerr;
        off = -1;
    }

    kfree(io);
    return off;
}

int iosched_write(inode_t* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!buf || !size || !inode || !inode->write)) {
        errno = EINVAL;
        return -1;
    }

    struct iorequest* io = (struct iorequest*) kmalloc(sizeof(struct iorequest), GFP_KERNEL);
    io->inode = inode;
    io->buffer = buf;
    io->size = size;
    io->position = pos;
    io->offset = 0;
    io->ioerr = 0;
    io->fn = inode->write;

    spinlock_init(&io->lock);
    spinlock_lock(&io->lock);

    list_push(inode->ioqueue, io);
    
    while(spinlock_trylock(&io->lock) != 0)
        iosched_rw(inode);


    off_t off = io->offset;
    
    if(unlikely(io->ioerr != 0)) {
        errno = io->ioerr;
        off = -1;
    }

    kfree(io);
    return off;
}

int iosched_init(void) {
    //iosched_blksiz = sysconfig("fs.iosched.blksize", SYSCONFIG_FORMAT_INT, 4096);
    return 0;
}
#endif