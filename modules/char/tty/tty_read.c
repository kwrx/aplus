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
static int tty_released = 0;

int tty_read_init() {
    tty_capslock =
    tty_shift =
    tty_alt =
    tty_control =
    tty_released = 0;

    return 0;
}

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
            case VK_RELEASED:
                tty_released = 1;
                continue;
            case VK_MENU:
            case VK_RMENU:
                tty_alt = !!!(tty_released);
                break;
            case VK_LSHIFT:
            case VK_RSHIFT:
                tty_shift = !!!(tty_released);
                break;
            case VK_LCONTROL:
            case VK_RCONTROL:
                tty_control = !!!(tty_control);
                break;
            case VK_CAPITAL:
                tty_capslock = tty_released ? tty_capslock : !tty_capslock;
                break;
            default:
                break;
        }
       


        if(unlikely(tty_released)) {
            tty_released = 0;
            continue;
        }

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
                    
                    if(ios->c_lflag & ECHO)
                        tty_write(inode, &ch, 1);
                }

                continue;
            default:
                if(tty_control) {
                    switch(ch) {
                        case 'c':
                            tty_write(inode, &ios->c_cc[VKILL], 1);
                            continue;
                        case 'd':
                            tty_write(inode, &ios->c_cc[VEOF], 1);
                            continue;
                        case 'q':
                            tty_write(inode, &ios->c_cc[VQUIT], 1);
                            continue;
                        case 'z':
                            tty_write(inode, &ios->c_cc[VINTR], 1);
                            continue;
                        default:
                            break;
                    }
                }
                
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