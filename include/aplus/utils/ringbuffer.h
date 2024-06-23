/*
 * Author(s):
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

#ifndef _APLUS_UTILS_RINGBUFFER_H
#define _APLUS_UTILS_RINGBUFFER_H

#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/ipc.h>
    #include <stdint.h>

typedef struct {

        uint8_t* buffer;
        size_t head;
        size_t tail;
        size_t size;
        uint8_t full;

        spinlock_t lock;

} ringbuffer_t;


__BEGIN_DECLS

void ringbuffer_init(ringbuffer_t* rb, size_t size);
void ringbuffer_destroy(ringbuffer_t* rb);
void ringbuffer_reset(ringbuffer_t* rb);

int ringbuffer_is_full(ringbuffer_t* rb);
int ringbuffer_is_empty(ringbuffer_t* rb);
size_t ringbuffer_available(ringbuffer_t* rb);
size_t ringbuffer_writeable(ringbuffer_t* rb);

ssize_t ringbuffer_write(ringbuffer_t* rb, const void* buf, size_t size);
ssize_t ringbuffer_read(ringbuffer_t* rb, void* buf, size_t size);

__END_DECLS

#endif
#endif
