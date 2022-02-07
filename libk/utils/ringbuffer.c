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
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>

#include <aplus/utils/ringbuffer.h>



void ringbuffer_init(ringbuffer_t* rb, size_t size, int flags) {
    
    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(size);

    rb->buffer = (uint8_t*) kmalloc(size, GFP_KERNEL);
    rb->size   = size;
    rb->head   = 0;
    rb->tail   = 0;
    rb->full   = 0;
    rb->flags  = flags;

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

    kprintf("ringbuffer: writing %ld bytes from %p\n", size, buf);

    __lock(&rb->lock, {

        for(size_t i = 0; i < size; i++) {

            rb->buffer[rb->head] = ((uint8_t*) buf) [i];

            if(ringbuffer_is_full(rb)) {
                rb->tail = (rb->tail + 1) % rb->size;

                // if(rb->flags & O_NONBLOCK)
                    //break;


                // rb->futex_rd_cond = 1;
                // futex_wait(current_task, &rb->futex_rd_cond, 0, NULL);

                // thread_suspend(current_task);
                // thread_postpone_resched(current_task);
                // for(;;);//schedule(1);

            }

            rb->head = (rb->head + 1) % rb->size;
            rb->full = (rb->head == rb->tail);

        }

        rb->futex_wr_cond = 0;

    });

    return size;

}


int ringbuffer_read(ringbuffer_t* rb, void* buf, size_t size) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(rb->buffer);


    size_t i;

    __lock(&rb->lock, {

        for(i = 0; i < size; i++) {

            if(ringbuffer_is_empty(rb)) {

                if(rb->flags & O_NONBLOCK)
                    break;


                // rb->futex_wr_cond = 1;
                // futex_wait(current_task, &rb->futex_wr_cond, 0, NULL);
                
                // thread_suspend(current_task);
                // thread_postpone_resched(current_task);

                // errno = EINTR;
                // i = -1;
                break;
                

                // for(;;);//schedule(1);

            }


            ((uint8_t*) buf) [i] = rb->buffer[rb->tail];

            rb->full = 0;
            rb->tail = (rb->tail + 1) % rb->size;
        }

        rb->futex_rd_cond = 0;

    });

    return i;

}