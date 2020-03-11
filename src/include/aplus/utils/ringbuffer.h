#ifndef _APLUS_UTILS_RINGBUFFER_H
#define _APLUS_UTILS_RINGBUFFER_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>

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


void ringbuffer_create(ringbuffer_t* rb, size_t size);
void ringbuffer_destroy(ringbuffer_t* rb);
void ringbuffer_reset(ringbuffer_t* rb);
int ringbuffer_is_full(ringbuffer_t* rb);
int ringbuffer_is_empty(ringbuffer_t* rb);
size_t ringbuffer_available(ringbuffer_t* rb);
int ringbuffer_write(ringbuffer_t* rb, const void* buf, size_t size);
int ringbuffer_read(ringbuffer_t* rb, void* buf, size_t size);


#endif
#endif