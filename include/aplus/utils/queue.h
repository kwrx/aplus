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

#ifndef _APLUS_UTILS_QUEUE_H
#define _APLUS_UTILS_QUEUE_H

#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/ipc.h>
    #include <stdint.h>

struct queue_element {

    void* element;
    int priority;

    struct queue_element* next;
};


typedef struct {

    struct queue_element* head;

    size_t size;
    spinlock_t lock;

} queue_t;



__BEGIN_DECLS

    #define queue_is_empty(queue) ((queue)->size == 0)


void queue_init(queue_t* queue);
void queue_destroy(queue_t* queue);
void queue_enqueue(queue_t* queue, void* element, int priority);
void queue_dequeue(queue_t* queue, void* element);
void* queue_top(queue_t*);
void* queue_pop(queue_t*);

__END_DECLS

#endif
#endif
