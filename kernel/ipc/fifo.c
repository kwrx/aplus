#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/vfs.h>
#include <libc.h>

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
            

        *buf++ = fifo->buffer[fifo->r_pos++ % BUFSIZ];
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
        fifo->buffer[(int) fifo->w_pos++ % BUFSIZ] = *buf++;

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

EXPORT(fifo_write);
EXPORT(fifo_read);
EXPORT(fifo_peek);
EXPORT(fifo_available);