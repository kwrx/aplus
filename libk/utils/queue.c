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

    struct queue_element* e = (struct queue_element*) kcalloc(1, sizeof(struct queue_element), GFP_KERNEL);

    e->element = element;
    e->priority = priority;
    e->next = next;

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

    while(queue->size > 0) {
        queue_pop(queue);
    }

}


void queue_enqueue(queue_t* queue, void* element, int priority) {

    DEBUG_ASSERT(queue);

    __lock(&queue->lock, {

        if(queue->size == 0) {

            queue->head = __queue_element(element, priority, NULL);

        } else {


            struct queue_element* tmp;
            struct queue_element* last = NULL;

            for(tmp = queue->head; tmp; last = tmp, tmp = tmp->next) {

                if(tmp->priority >= priority)
                    continue;

                tmp->next = __queue_element(element, priority, tmp->next);
                            __queue_swap(tmp, tmp->next);

            }


            DEBUG_ASSERT(last);

            if(!tmp) {
                last->next = __queue_element(element, priority, NULL);
            }

        }

        queue->size += 1;

    });

}


void queue_dequeue(queue_t* queue, void* element) {

    DEBUG_ASSERT(queue);

    if(queue->size == 0)
        return;

    if(queue_top(queue) == element) {
      
        queue_pop(element);

    } else {

        __lock(&queue->lock, {


            struct queue_element* tmp;
            struct queue_element* last;

            for(last = queue->head, tmp = queue->head->next; tmp; last = tmp, tmp = tmp->next) {

                if(tmp->element != element)
                    continue;


                last->next = tmp->next;

                kfree(tmp);

            }


            queue->size -= 1;

        });

    }

}


void* queue_top(queue_t* queue) {

    DEBUG_ASSERT(queue);
    
    if(queue->size == 0)
        return NULL;

    DEBUG_ASSERT(queue->head);
    return queue->head->element;

}


void* queue_pop(queue_t* queue) {

    DEBUG_ASSERT(queue);

    if(queue->size == 0)
        return NULL;

    DEBUG_ASSERT(queue->head);


    struct queue_element* top = queue->head;
    void* element = top->element;


    __lock(&queue->lock, {

        queue->head = queue->head->next;
        queue->size -= 1;

    });


    kfree(top);
    return element;

}
