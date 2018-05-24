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


static void tty_signal(struct tty_context* tio, int sig) {
    if(tio->ios.c_lflag & ISIG)
        sys_kill(-tio->pgrp, sig);
}

int tty_output_write(struct inode* inode, void* ptr, off_t pos, size_t len) {
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
    uint8_t* buf = (uint8_t*) ptr;
    size_t p = 0;
    

    if((tio->ios.c_lflag & TOSTOP) && (current_task->pgid != tio->pgrp))
        sys_exit((1 << 31) | W_STOPCODE(SIGTTOU));
    
    
    while(p < len) {
#ifdef OLCUC
        if(tio->ios.c_oflag & OLCUC)
            *buf = (*buf >= 'a' && *buf <= 'z')
                    ? *buf - 32
                    : *buf;
#endif


        if(*buf == '\r' && tio->ios.c_iflag & ICRNL)
            *buf = '\n';
     
     
        if(*buf == tio->ios.c_cc[VEOF])
            break;
        else if(*buf == tio->ios.c_cc[VERASE])
            { *buf-- = '\b'; p--; }
        else if(*buf == tio->ios.c_cc[VINTR])
            tty_signal(tio, SIGINT);
        else if(*buf == tio->ios.c_cc[VKILL])
            tty_signal(tio, SIGKILL);
        else if(*buf == tio->ios.c_cc[VQUIT])
            tty_signal(tio, SIGQUIT);
        else if(*buf == tio->ios.c_cc[VSUSP])
            tty_signal(tio, SIGTSTP);
        else if(*buf == tio->ios.c_cc[VSTOP])
            tio->output = (tio->ios.c_iflag & IXON) ? 0 : tio->output;
        else if(*buf == tio->ios.c_cc[VSTART])
            tio->output = (tio->ios.c_iflag & IXON) ? 1 : tio->output;
        else
            if(tio->outlen < sizeof(tio->outbuf))
                tio->outbuf[tio->outlen++] = *buf;
        
        
        buf++;
        p++;
    }
    

    if(tio->output)
        __tty_flush(tio);

    return p;
}