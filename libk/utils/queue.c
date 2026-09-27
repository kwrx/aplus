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

#include <aplus/utils/queue.h>



static inline void __queue_swap(struct queue_element* a, struct queue_element* b) {

    DEBUG_ASSERT(a);
    DEBUG_ASSERT(b);


    int prio = a->priority;

    a->priority = b->priority;
    b->priority = prio;


    void* elem = a->element;

    a->element = b->element;
    b->element = elem;
}


static inline struct queue_element* __queue_element(void* element, int priority, struct queue_element* next) {

    struct queue_element* e = (struct queue_element*)kcalloc(1, sizeof(struct queue_element), GFP_KERNEL);

    e->element  = element;
    e->priority = priority;
    e->next     = next;

    return e;
}



void queue_init(queue_t* queue) {

    DEBUG_ASSERT(queue);

    queue->head = NULL;
    queue->size = 0;

    spinlock_init(&queue->lock);
}


void queue_destroy(queue_t* queue) {

    DEBUG_ASSERT(queue);

    while (queue->size > 0) {
        queue_pop(queue);
    }
}


void queue_enqueue(queue_t* queue, void* element, int priority) {

    DEBUG_ASSERT(queue);

    scoped_lock(&queue->lock) {
        if (queue->size == 0) {

            queue->head = __queue_element(element, priority, NULL);

        } else {


            struct queue_element* tmp;
            struct queue_element* last = NULL;

            for (tmp = queue->head; tmp; last = tmp, tmp = tmp->next) {

                if (tmp->priority >= priority)
                    continue;

                tmp->next = __queue_element(element, priority, tmp->next);
                __queue_swap(tmp, tmp->next);
            }


            DEBUG_ASSERT(last);

            if (!tmp) {
                last->next = __queue_element(element, priority, NULL);
            }
        }

        queue->size += 1;
    }
}


void* queue_pop(queue_t* queue) {

    DEBUG_ASSERT(queue);


    struct queue_element* top = NULL;
    void* element             = NULL;

    scoped_lock(&queue->lock) {

        if (queue->size == 0)
            break;

        DEBUG_ASSERT(queue->head);

        top     = queue->head;
        element = top->element;

        queue->head = top->next;
        queue->size -= 1;
    }


    if (top)
        kfree(top);

    return element;
}
