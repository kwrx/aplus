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
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/vfs.h>
#include <libc.h>


int fifo_init(fifo_t* fifo, size_t size, int async) {

    fifo->w_pos =
    fifo->r_pos = 0;

    fifo->async = async;
    fifo->size = size;

    fifo->buffer = kmalloc(size, GFP_KERNEL);
    KASSERT(fifo->buffer);

    mutex_init(&fifo->r_lock, MTX_KIND_DEFAULT, "fifo");
    mutex_init(&fifo->w_lock, MTX_KIND_DEFAULT, "fifo");

    return 0;
}


int fifo_read(fifo_t* fifo, void* ptr, size_t len) {
    if(unlikely(!fifo)) {
        errno = EINVAL;
        return 0;
    }

    if(unlikely(!len))
        return 0;

    uint8_t* buf = ptr;

    int i;
    for(i = 0; i < len; i++) {
        if(fifo->async) {
            if(unlikely(!(fifo->w_pos > fifo->r_pos)))
                return i;
        } else
            ipc_wait(!(fifo->w_pos > fifo->r_pos));
            

        *buf++ = fifo->buffer[fifo->r_pos++ % fifo->size];
    }

    return len;
}

int fifo_write(fifo_t* fifo, void* ptr, size_t len) {
    if(unlikely(!fifo)) {
        errno = EINVAL;
        return 0;
    }

    if(unlikely(!len))
        return 0;

    uint8_t* buf = ptr;

    int i;
    for(i = 0; i < len; i++)
        fifo->buffer[(int) fifo->w_pos++ % fifo->size] = *buf++;

    return len;
}

int fifo_peek(fifo_t* fifo, size_t len) {

    int i = len;
    while(i--)
        fifo->w_pos--;
    
    if(fifo->r_pos > fifo->w_pos)
        fifo->r_pos = fifo->w_pos;
            
    return i;
}


int fifo_available(fifo_t* fifo) {
    if(unlikely(!fifo)) {
        errno = EINVAL;
        return 0;
    }

    return fifo->w_pos - fifo->r_pos;
}

EXPORT(fifo_init);
EXPORT(fifo_write);
EXPORT(fifo_read);
EXPORT(fifo_peek);
EXPORT(fifo_available);