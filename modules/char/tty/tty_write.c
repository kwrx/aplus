#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <libc.h>

#include "tty.h"



int tty_write(struct inode* inode, void* ptr, size_t len) {
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


    struct termios* ios = (struct termios*) inode->userdata;
    uint8_t* buf = (uint8_t*) ptr;
    size_t p = 0;
    
    
    
    while(p < len) {
#ifdef OLCUC
        if(ios->c_oflag & OLCUC)
            *buf = (*buf >= 'a' && *buf <= 'z')
                    ? *buf - 32
                    : *buf;
#endif

     
        if(*buf == ios->c_cc[VEOF])
            break;
        else if(*buf == ios->c_cc[VERASE])
            kprintf(USER, "\b");
        else if(*buf == ios->c_cc[VINTR])
            sys_kill(current_task->pid, SIGINT);
        else if(*buf == ios->c_cc[VKILL])
            sys_kill(current_task->pid, SIGKILL);
        else if(*buf == ios->c_cc[VQUIT])
            sys_kill(current_task->pid, SIGQUIT);
        else
            kprintf(USER, "%c", *buf);
        
        buf++;
        p++;
    }
    
    return p;
}