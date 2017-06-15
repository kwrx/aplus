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

    register uint8_t* buf = ptr;

    int i;
    for(i = 0; i < len; i++) {
        while(!(fifo->w_pos > fifo->r_pos))
            sys_yield();

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

    register uint8_t* buf = ptr;

    int i;
	for(i = 0; i < len; i++)
		fifo->buffer[(int) fifo->w_pos++ % BUFSIZ] = *buf++;

    return len;
}


int fifo_available(fifo_t* fifo) {
    if(unlikely(!fifo)) {
        errno = EINVAL;
        return 0;
    }

    return fifo->w_pos - fifo->r_pos;
}