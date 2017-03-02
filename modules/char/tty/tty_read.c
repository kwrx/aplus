#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <libc.h>

#include "tty.h"


int tty_read(struct inode* inode, void* ptr, size_t len) {
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
        
    int fd = sys_open(TTY_DEFAULT_INPUT_DEVICE, O_RDONLY, 0);
    if(fd < 0) {
        errno = EIO;
        return E_ERR;
    }
    
        
    struct termios* ios = (struct termios*) inode->userdata;
    register uint8_t* buf = (uint8_t*) ptr;
    register int p = 0;
    
    
    while(p < len) {
        uint8_t ch;
        if(sys_read(fd, &ch, sizeof(ch)) == 0) {
            errno = EIO;
            return E_ERR;
        }
       
 
               
        switch(ch) {
            case '\b':
                if(p > 0) {
                    *--buf = 0;
                    p--;
                    
                    if(ios->c_lflag & ECHO)
                        tty_write(inode, &ch, 1);
                }

                continue;
            default:
                *buf++ = ch;
                break;
        }
        
        if(ios->c_lflag & ECHO)
            tty_write(inode, &ch, 1);
                    
        if(!(ios->c_iflag & IGNCR)) {
            if(ch == '\n')
                break;
        }
        
        p++;           
    }
    

    if(p < len) {
        if(!(ios->c_iflag & IGNCR)) {
            *buf++ = '\n';
            p++;
        }
    }
    
    *buf++ = 0;
    
    
    sys_close(fd);
    return p;
}