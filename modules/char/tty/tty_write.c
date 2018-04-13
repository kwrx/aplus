#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <libc.h>

#include "tty.h"



void __tty_flush(struct tty_context* tio) {
    int fd = sys_open(TTY_DEFAULT_OUTPUT_DEVICE, O_WRONLY, 0);
    if(likely(fd >= 0)) {
        sys_write(fd, tio->outbuf, tio->outlen);
        sys_close(fd);
    }

    tio->outlen = 0;
}

int tty_write(struct inode* inode, void* ptr, off_t pos, size_t len) {
    if(unlikely(!inode || !ptr)) {
        errno = EINVAL;
        return E_ERR;
    }
    
    if(unlikely(!inode->userdata)) {
        errno = EINVAL;
        return E_ERR;
    }
    
    if(unlikely(!len))
        return 0;


    struct tty_context* tio = (struct tty_context*) inode->userdata;
    uint8_t* buf = (uint8_t*) ptr;
    size_t p = 0;
    
    
    
    while(p < len) {
#ifdef OLCUC
        if(tio->ios.c_oflag & OLCUC)
            *buf = (*buf >= 'a' && *buf <= 'z')
                    ? *buf - 32
                    : *buf;
#endif

     
        if(*buf == tio->ios.c_cc[VEOF])
            break;
        else if(*buf == tio->ios.c_cc[VERASE])
            kprintf("\b");
        else if(*buf == tio->ios.c_cc[VINTR])
            sys_kill(-tio->pgrp, SIGINT);
        else if(*buf == tio->ios.c_cc[VKILL])
            sys_kill(-tio->pgrp, SIGKILL);
        else if(*buf == tio->ios.c_cc[VQUIT])
            sys_kill(-tio->pgrp, SIGQUIT);
        else if(*buf == tio->ios.c_cc[VSTOP])
            tio->output = 0;
        else if(*buf == tio->ios.c_cc[VSTART])
            tio->output = 1;
        else
            tio->outbuf[tio->outlen++] = *buf;
        
        
        buf++;
        p++;
    }
    

    if(tio->output)
        __tty_flush(tio);

    return p;
}