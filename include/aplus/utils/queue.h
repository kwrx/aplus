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

#define queue_is_empty(queue)   \
    ((queue)->size == 0)


void queue_init(queue_t* queue);
void queue_destroy(queue_t* queue);
void queue_enqueue(queue_t* queue, void* element, int priority);
void queue_dequeue(queue_t* queue, void* element);
void* queue_top(queue_t*);
void* queue_pop(queue_t*);

__END_DECLS

#endif
#endif