#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/input.h>
#include <libc.h>

#include "tty.h"


static int tty_capslock = 0;
static int tty_shift = 0;
static int tty_alt = 0;
static int tty_control = 0;
static char tty_keymap[1024];

int tty_read_init() {
    tty_capslock =
    tty_shift =
    tty_alt =
    tty_control = 0;

    memset(tty_keymap, 0, sizeof(tty_keymap));
    return 0;
}

int tty_load_keymap(char* keymap) {
    static char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    sprintf(buf, PATH_KEYMAPS "/%s", keymap);
    int fd = sys_open(buf, O_RDONLY, 0);
    if(fd < 0)
        return -1;

    if(sys_read(fd, tty_keymap, 1024) != 1024) {
        sys_close(fd);
        return -1;
    }


    kprintf(INFO "tty: keymap \'%s\'\n", keymap);
    sys_close(fd);
    return 0;
}

int tty_read(struct inode* inode, void* ptr, off_t pos, size_t len) {
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
    
        
    struct tty_context* tio = (struct tty_context*) inode->userdata;
    register uint8_t* buf = (uint8_t*) ptr;
    register int p = 0;
    
    
    while(p < len) {
        keyboard_t k;
        if(sys_read(fd, &k, sizeof(keyboard_t)) == 0) {
            errno = EIO;
            return E_ERR;
        }

        switch(k.vkey) {
            case KEY_LEFTALT:
            case KEY_RIGHTALT:
                tty_alt = k.down;
                break;
            case KEY_LEFTSHIFT:
            case KEY_RIGHTSHIFT:
                tty_shift = k.down;
                break;
            case KEY_LEFTCTRL:
            case KEY_RIGHTCTRL:
                tty_control = k.down;
                break;
            case KEY_CAPSLOCK:
                tty_capslock = !(k.down) ? tty_capslock : !tty_capslock;
                break;
            default:
                break;
        }

        if(!k.down)
            continue;


        uint8_t ch = k.vkey;

        if(tty_alt && tty_shift)
            ch = tty_keymap[ch + 768];
        else if(tty_alt)
            ch = tty_keymap[ch + 512];
        else if(tty_shift)
            ch = tty_keymap[ch + 256];
        else
            ch = tty_keymap[ch];


        if(tty_capslock) {
            if(tty_shift)
                ch += ch >= 'A' && ch <= 'Z' ? 32 : 0;
            else
                ch -= ch >= 'a' && ch <= 'z' ? 32 : 0;
        }


        switch(ch) {
            case '\0':
                continue;
            case '\b':
                if(p > 0) {
                    *--buf = 0;
                    p--;
                    
                    if(tio->ios.c_lflag & ECHO)
                        tty_write(inode, &ch, 0, 1);
                }

                continue;
            default:
                if(tty_control) {
                    switch(ch) {
                          case 'a' ... 'z':
                            ch -= 32;
                          case 'A' ... 'Z':
                            tty_write(inode, "^", 0, 1);
                            tty_write(inode, &ch, 0, 1);
                            tty_write(inode, "\n", 0, 1);
                            continue;
                        default:
                            break;
                    }
                }
                
                *buf++ = ch;
                break;
        }
        
        if(tio->ios.c_lflag & ECHO)
            tty_write(inode, &ch, 0, 1);
                    
        if(!(tio->ios.c_iflag & IGNCR)) {
            if(ch == '\n')
                break;
        }
        
        p++;           
    }
    

    if(p < len) {
        if(!(tio->ios.c_iflag & IGNCR)) {
            *buf++ = '\n';
            p++;
        }
    }
    
    *buf++ = 0;
    
    
    sys_close(fd);
    return p;
}