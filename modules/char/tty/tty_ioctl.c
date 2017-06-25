#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/input.h>
#include <libc.h>


#include <sys/ioctl.h>
#include <sys/termio.h>
#include <sys/termios.h>
#include "tty.h"



int tty_ioctl(struct inode* inode, int req, void* data) {
    if(!inode) {
        errno = EINVAL;
        return -1;
    }


    struct tty_context* tio = (struct tty_context*) inode->userdata;
    if(unlikely(!tio)) {
        errno = ENOTTY;
        return -1;
    }



    #define _p(x)   if(unlikely(!x)) { errno = EINVAL; return -1; }

    switch(req) {
        case TIOCGETA:
            _p(data);
            memcpy(data, &tio->ios, sizeof(struct termios));
            break;
        case TIOCSETA:
        case TIOCSETAW:
        case TIOCSETAF:
            _p(data);
            memcpy(&tio->ios, data, sizeof(struct termios));
            break;

        case TCGETA:
            _p(data);
            memcpy(data, &tio->io, sizeof(struct termio));
            break;

        case TIOCFLUSH:
        case TIOCDRAIN:
            __tty_flush(tio);
            break;

        case TIOCGETD:
            _p(data);
            *(int*) data = tio->lined;
            break;
        case TIOCSETD:
            _p(data);
            tio->lined = *(int*) data;
            break; 
        
        case TIOCGWINSZ:
            _p(data);
            memcpy(data, &tio->winsize, sizeof(struct winsize));
            break;
        case TIOCSWINSZ:
            //_p(data);
            //memcpy(&tio->winsize, data, sizeof(struct winsize));

            sys_kill(current_task->pid, SIGWINCH);
            break;
        
        case TCSBRK:
        case TIOCSBRK:
        case TIOCCBRK:
            break;

        case FIONREAD:
        case TIOCOUTQ:
            _p(data);
            *(int*) data = 0;
            break;


        case TIOCSTART:
            tio->output = 1;
            break;

        case TIOCSTOP:
            tio->output = 0;
            break;

        case TIOCSPGRP:
            _p(data);
            tio->pgrp = *(int*) data;
            break;
        case TIOCGPGRP:
            _p(data);
            *(int*) data = tio->pgrp;
            break;

        case TIOCLKEYMAP:
            _p(data);
            return tty_load_keymap(data);
    
        default:
            errno = ENOSYS;
            return -1;
    }

    return 0;
}