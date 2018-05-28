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

#include <aplus/base.h>
#include <aplus/utils/unicode.h>

#include "tty.h"


#define KEYMAP_SIZE \
    (16 * NR_KEYS * sizeof(uint16_t))


#define K_SHIFT             (1 << 0)
#define K_CTRL              (1 << 1)
#define K_ALT               (1 << 2)
#define K_ALTGR             (1 << 3)


static int tty_capslock = 0;
static int tty_keys = 0;

static struct {
    uint16_t plain[NR_KEYS];
    uint16_t shift[NR_KEYS];
    uint16_t altgr[NR_KEYS];
    uint16_t shift_altgr[NR_KEYS];
    
    uint16_t ctrl[NR_KEYS];
    uint16_t shift_ctrl[NR_KEYS];
    uint16_t altgr_ctrl[NR_KEYS];
    uint16_t altgr_shift_ctrl[NR_KEYS];

    uint16_t alt[NR_KEYS];
    uint16_t shift_alt[NR_KEYS];
    uint16_t altgr_alt[NR_KEYS];
    uint16_t altgr_shift_alt[NR_KEYS];

    uint16_t ctrl_alt[NR_KEYS];
    uint16_t shift_ctrl_alt[NR_KEYS];
    uint16_t altgr_ctrl_alt[NR_KEYS];
    uint16_t altgr_shift_ctrl_alt[NR_KEYS];
} tty_keymap;


char* __kmap_spec[] = { 
    NULL, "\n", NULL, NULL, NULL,
    "\b", NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
};

char* __kmap_kp[] = {
    "0", "1", "2", "3", "4",
    "5", "6", "7", "8", "9",
    "+", "-", "*", "/", "\n",
    ",", ".", "+-", "(", ")"
};

char* __kmap_dead[] = {
    "`", "´", "^", "~", "¨", "¸"
};
    
char* __kmap_cur[] = {
    "\e[B", "\e[D", "\e[C", "\e[A"
};

char** __kmap[] = {
    NULL,
    NULL,
    __kmap_spec,
    __kmap_kp,
    __kmap_dead,
    NULL,
    __kmap_cur,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


int tty_read_init() {
    tty_capslock =
    tty_keys = 0;

    memset(&tty_keymap, 0, sizeof(tty_keymap));
    return 0;
}

int tty_load_keymap(char* keymap) {
    static char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    sprintf(buf, PATH_KEYMAPS "/%s", keymap);
    int fd = sys_open(buf, O_RDONLY, 0);
    if(fd < 0)
        return -1;

    if(sys_read(fd, &tty_keymap, KEYMAP_SIZE) != KEYMAP_SIZE) {
        kprintf(ERROR "tty: I/O error on reading keymap %s\n", keymap);
        sys_close(fd);
        return -1;
    }


    kprintf(INFO "tty: keymap \'%s\'\n", keymap);
    sys_close(fd);
    return 0;
}


static uint8_t process_keyboard(inode_t* inode, struct tty_context* tio, int fd, int* pos) {
    keyboard_t k;
    if(sys_read(fd, &k, sizeof(keyboard_t)) == 0) {
        errno = EIO;
        return 0;
    }


    switch(k.vkey) {
        case KEY_LEFTALT:
            tty_keys = !!(k.down) ? tty_keys | K_ALT : tty_keys & ~K_ALT;
            return 0;
        case KEY_RIGHTALT:
            tty_keys = !!(k.down) ? tty_keys | K_ALTGR : tty_keys & ~K_ALTGR;
            return 0;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            tty_keys = !!(k.down) ? tty_keys | K_SHIFT : tty_keys & ~K_SHIFT;
            return 0;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            tty_keys = !!(k.down) ? tty_keys | K_CTRL : tty_keys & ~K_CTRL;
            return 0;
        case KEY_CAPSLOCK:
            tty_capslock = !(k.down) ? tty_capslock : !tty_capslock;
            return 0;
        default:
            break;
    }

    if(!k.down)
        return 0;


    int16_t key;
    uint8_t ch;

    switch(tty_keys) {
        #define _(x, y)    \
            case x : { key = tty_keymap.y[k.vkey] & 0x0FFF; break; }

        _(0, plain);
        _(K_SHIFT, shift);
        _(K_ALTGR, altgr);
        _(K_SHIFT | K_ALTGR, shift_altgr);
        _(K_CTRL, ctrl);
        _(K_SHIFT | K_CTRL, shift_ctrl);
        _(K_ALTGR | K_CTRL, altgr_ctrl);
        _(K_ALTGR | K_SHIFT | K_CTRL, altgr_shift_ctrl);
        _(K_ALT, alt);
        _(K_SHIFT | K_ALT, shift_alt);
        _(K_ALTGR | K_ALT, altgr_alt);
        _(K_ALTGR | K_SHIFT | K_ALT, altgr_shift_alt);
        _(K_CTRL | K_ALT, ctrl_alt);
        _(K_SHIFT | K_CTRL | K_ALT, shift_ctrl_alt);
        _(K_ALTGR | K_CTRL | K_ALT, altgr_ctrl_alt);
        _(K_ALTGR | K_SHIFT | K_CTRL | K_ALT, altgr_shift_ctrl_alt);
        
        default:
            key = 0;
            break;

        #undef _
    }



    ch = KVAL(key);
    switch(KTYP(key)) {
        case KT_LATIN:
        case KT_LETTER:
            if(tty_capslock) {
                if(tty_keys & K_SHIFT)
                    ch += ch >= 'A' && ch <= 'z' ? 32 : 0;
                else
                    ch -= ch >= 'a' && ch <= 'Z' ? 32 : 0;
            }
        case KT_ASCII:
            break;

        case KT_SPEC:
        case KT_PAD:
            if(!__kmap[KTYP(key)] || !__kmap[KTYP(key)][KVAL(key)])
                return 0;

            ch = __kmap[KTYP(key)][KVAL(key)][0];
            break;

        default:
            if(!__kmap[KTYP(key)] || !__kmap[KTYP(key)][KVAL(key)])
                return 0;


            fifo_write(&tio->in, __kmap[KTYP(key)][KVAL(key)], strlen(__kmap[KTYP(key)][KVAL(key)]));
            *pos = *pos + strlen(__kmap[KTYP(key)][KVAL(key)]);

            if(tio->ios.c_lflag & ECHO)
                tty_output_write(inode, __kmap[KTYP(key)][KVAL(key)], 0, strlen(__kmap[KTYP(key)][KVAL(key)]));

            return 0;
    }

    return ch;
}

int tty_read(struct inode* inode, void* ptr, off_t pos, size_t len) {
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
    int p = 0;


    if(tio->pgrp != current_task->pgid) 
        sys_exit((1 << 31) | W_STOPCODE(SIGTTIN));

    

    int fd = sys_open(TTY_DEFAULT_INPUT_DEVICE, O_RDONLY, 0);
    if(fd < 0) {
        errno = EIO;
        return -1;
    }


    if(fifo_available(&tio->in)) {
        char tmp[BUFSIZ];
        for(int j = 0; (j = fifo_read(&tio->in, tmp, sizeof(tmp))) > 0;)
            fifo_write(&tio->uin, tmp, j);
    }
    

    while(p < len) {
        uint8_t ch;
        if(fifo_available(&tio->uin))
            fifo_read(&tio->uin, &ch, sizeof(ch));
        else
            ch = process_keyboard(inode, tio, fd, &p);



        switch(ch) {
            case 0:
                continue;
            case '\b':
            case '\x7f':
                if(tio->ios.c_lflag & ICANON) {
                    if(p > 0) {
                        fifo_peek(&tio->in, 1);
                        p--;
                        
                        if(tio->ios.c_lflag & ECHOE)
                            ch = '\b';
                        else
                            ch = '\x7f';

                        if(tio->ios.c_lflag & ECHO)
                            tty_output_write(inode, &ch, 0, 1);
                    }

                    continue;
                } 

                /* No processing */
                ch = '\b';
                break;
            default:
                break;
        }


        if(unlikely(ch < 32)) {
            for(int i = 0; i < NCCS; i++) {
                if(ch != tio->ios.c_cc[i])
                    continue;


                fifo_write(&tio->in, &ch, 1);
                p++;

                ch = 0;
                break;
            }

            if(unlikely(!ch))
                continue;
        }



        if(!(tio->ios.c_iflag & IGNCR)) {
            if(ch == '\n')
                break;
        }


        char utf8[UTF8_MAX_LENGTH];
        size_t utf8len = ucs2_to_utf8((int32_t) ch, utf8);
        
        fifo_write(&tio->in, utf8, utf8len);
        p += utf8len;

        if(tio->ios.c_lflag & ECHO)
            tty_output_write(inode, utf8, 0, utf8len);           
    }



    if(p < len) {
        if(!(tio->ios.c_iflag & IGNCR)) {
            char ch = '\n';
            fifo_write(&tio->in, &ch, 1);
            p++;


            if((tio->ios.c_lflag & ECHO) || (tio->ios.c_lflag & ICANON && tio->ios.c_lflag & ECHONL))
                tty_output_write(inode, &ch, 0, 1);
        }
    } else
        p = len;
    


    if(fifo_available(&tio->in))
        fifo_read(&tio->in, buf, p);
    
    
    sys_close(fd);
    return p;
}