/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/input.h>
#include <libc.h>


#include <sys/ioctl.h>
#include <termios.h>
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
        case TCGETA:
            _p(data);
            memcpy(data, &tio->ios, sizeof(struct termios));
            break;
        case TCSETA:
        case TCSETAW:
        case TCSETAF:
            _p(data);
            memcpy(&tio->ios, data, sizeof(struct termios));
            break;

        case TCXONC:
            __tty_drain(tio);
            break;

        case TCFLSH:
            __tty_flush(tio, *(int*) data);
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
            _p(data);
            memcpy(&tio->winsize, data, sizeof(struct winsize));

            sys_kill(-1, SIGWINCH);
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


        /*case TIOCSTART:
            tio->output = 1;
            break;

        case TIOCSTOP:
            tio->output = 0;
            break;
        */
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