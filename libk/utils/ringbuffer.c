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
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <fcntl.h>
#include <stdint.h>

#include <aplus/utils/ringbuffer.h>


//? ringbuffer_read() and ringbuffer_write() report their outcome through the
//? return value alone and never touch errno: errno is per-cpu
//? (current_cpu->errno) and the caller may be preempted before it reads it.
//?
//?     > 0         number of bytes transferred, possibly less than requested
//?     0           nothing was asked for (size == 0)
//?     -EAGAIN     would block: the buffer is empty (read) or full (write)
//?     -EIO        the buffer has been destroyed underneath the caller


int ringbuffer_init(ringbuffer_t* rb, size_t size) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(size);

    rb->buffer = (uint8_t*)kmalloc(size, GFP_KERNEL);

    if (unlikely(!rb->buffer))
        return -ENOMEM;

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->full = 0;

    spinlock_init(&rb->lock);

    return 0;
}


void ringbuffer_destroy(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);

    scoped_lock(&rb->lock) {

        if (likely(rb->buffer))
            kfree(rb->buffer);

        rb->size   = 0;
        rb->head   = 0;
        rb->tail   = 0;
        rb->full   = 0;
        rb->buffer = NULL;
    }
}


void ringbuffer_reset(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);

    scoped_lock(&rb->lock) {
        rb->head = 0;
        rb->tail = 0;
        rb->full = 0;
    }
}


//? The query helpers below must stay lock-free: ringbuffer_read() and
//? ringbuffer_write() call them with rb->lock already held, and spinlock
//? ownership is per-task, so re-entering would trip the DEADLOCK panic.
//? They also tolerate a destroyed buffer rather than asserting on it.

int ringbuffer_is_full(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);

    if (unlikely(!rb->buffer))
        return 0;

    return rb->full;
}


int ringbuffer_is_empty(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);

    if (unlikely(!rb->buffer))
        return 1;

    return (!rb->full && (rb->head == rb->tail));
}


size_t ringbuffer_available(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);

    if (unlikely(!rb->buffer))
        return 0;

    if (rb->full)
        return rb->size;

    if (rb->head >= rb->tail)
        return rb->head - rb->tail;

    return rb->head - rb->tail + rb->size;
}


size_t ringbuffer_writeable(ringbuffer_t* rb) {

    DEBUG_ASSERT(rb);

    if (unlikely(!rb->buffer))
        return 0;

    if (rb->full)
        return 0;

    if (rb->head < rb->tail)
        return rb->tail - rb->head;

    return rb->size - (rb->head - rb->tail);
}


ssize_t ringbuffer_write(ringbuffer_t* rb, const void* buf, size_t size) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(buf);


    if (unlikely(size == 0))
        return 0;


    ssize_t e = -EIO;

    scoped_lock(&rb->lock) {
        if (unlikely(rb->buffer == NULL)) {

            e = -EIO;

        } else {

            //? Short writes are legal and are what keeps a write larger than
            //? the buffer from being unsatisfiable forever; only a completely
            //? full buffer is a would-block condition.

            size_t n = ringbuffer_writeable(rb);

            if (n == 0) {

                e = -EAGAIN;

            } else {

                if (n > size)
                    n = size;

                for (size_t i = 0; i < n; i++) {

                    rb->buffer[rb->head] = ((uint8_t*)buf)[i];

                    rb->head = (rb->head + 1) % rb->size;
                    rb->full = (rb->head == rb->tail);
                }

                e = (ssize_t)n;
            }
        }
    }

    return e;
}


ssize_t ringbuffer_read(ringbuffer_t* rb, void* buf, size_t size) {

    DEBUG_ASSERT(rb);
    DEBUG_ASSERT(buf);


    if (unlikely(size == 0))
        return 0;


    ssize_t e = -EIO;

    scoped_lock(&rb->lock) {
        if (unlikely(rb->buffer == NULL)) {

            e = -EIO;

        } else {

            if (ringbuffer_is_empty(rb)) {

                e = -EAGAIN;

            } else {

                size_t i = 0;

                for (; i < size && !ringbuffer_is_empty(rb); i++) {

                    ((uint8_t*)buf)[i] = rb->buffer[rb->tail];

                    rb->full = 0;
                    rb->tail = (rb->tail + 1) % rb->size;
                }

                e = (ssize_t)i;
            }
        }
    }

    return e;
}
