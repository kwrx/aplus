#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <libc.h>

#include "tty.h"



int tty_input_write(struct inode* inode, void* ptr, off_t pos, size_t len) {
    if(unlikely(!inode || !ptr)) {
        errno = EINVAL;
        return -1;
    }
    
    if(unlikely(!inode->userdata)) {
        errno = EINVAL;
        return -1;
    }
    
    if(unlikely(!len))
        return 0;

    struct tty_context* tio = (struct tty_context*) inode->userdata;
    return fifo_write(&tio->in, ptr, len);
}