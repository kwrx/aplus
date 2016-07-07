#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <libc.h>

#include "tty.h"


static void __wch(char ch) {
    
}


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

    int fd = sys_open("/dev/fb0", O_WRONLY, 0666);
    if(unlikely(fd) < 0) {
        errno = ENODEV;
        return E_ERR;
    }



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
            { char ch = '\b'; sys_write(fd, &ch, 1); }
        else if(*buf == ios->c_cc[VINTR])
            sys_kill(current_task->pid, SIGINT);
        else if(*buf == ios->c_cc[VKILL])
            sys_kill(current_task->pid, SIGKILL);
        else if(*buf == ios->c_cc[VQUIT])
            sys_kill(current_task->pid, SIGQUIT);
        else
            sys_write(fd, buf, 1);
        
        buf++;
        p++;
    }
    
    sys_close(fd);
    return p;
}