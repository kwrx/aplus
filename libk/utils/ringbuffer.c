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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <stdint.h>

#include <aplus/utils/ringbuffer.h>



void ringbuffer_init(ringbuffer_t* rb, size_t size) {
    
    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(size);

    rb->buffer = (uint8_t*) kmalloc(size, GFP_KERNEL);
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->full = 0;

    spinlock_init(&rb->lock);

}


void ringbuffer_destroy(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);

    __lock(&rb->lock, {
        
        kfree(rb->buffer);

        rb->size = 0;
        rb->head = 0;
        rb->tail = 0;
        rb->buffer = NULL;

    });

}


void ringbuffer_reset(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);

    __lock(&rb->lock, {
        
        rb->head = 0;
        rb->tail = 0;
        rb->full = 0;

    });

}


int ringbuffer_is_full(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);

    return rb->full;

}


int ringbuffer_is_empty(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);

    return (!rb->full & (rb->head == rb->tail));

}


size_t ringbuffer_available(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);

    if(rb->full)
        return rb->size;

    if(rb->head >= rb->tail)
        return rb->head - rb->tail;
    
    return rb->head - rb->tail + rb->size;

}


int ringbuffer_write(ringbuffer_t* rb, const void* buf, size_t size) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);


    __lock(&rb->lock, {

        for(size_t i = 0; i < size; i++) {

            rb->buffer[rb->head] = ((uint8_t*) buf) [i];

            if(rb->full)
                rb->tail = (rb->tail + 1) % rb->size;

            rb->head = (rb->head + 1) % rb->size;
            rb->full = (rb->head == rb->tail);

        }

    });

    return size;

}


int ringbuffer_read(ringbuffer_t* rb, void* buf, size_t size) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);


    size_t i;

    __lock(&rb->lock, {

        for(i = 0; i < size; i++) {

            if(ringbuffer_is_empty(rb))
                break;


            ((uint8_t*) buf) [i] = rb->buffer[rb->tail];

            rb->full = 0;
            rb->tail = (rb->tail + 1) % rb->size;
        }

    });

    return i;

}