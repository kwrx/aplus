#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/sysconfig.h>
#include <libc.h>

#include "tty.h"

MODULE_NAME("char/tty");
MODULE_DEPS("system/events");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



int init(void) {
    tty_read_init();


    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("tty", 0, S_IFCHR | 0666)) == NULL))
        return -1;


    struct tty_context* tio = (struct tty_context*) kmalloc(sizeof(struct tty_context), GFP_KERNEL);
    if(unlikely(!tio)) {
        kprintf(ERROR "tty: no memory left!");
        return -1;
    }

    memset(tio, 0, sizeof(struct tty_context));
    tio->ios.c_iflag |= ICRNL | IXON;
    tio->ios.c_oflag |= OPOST;
    tio->ios.c_cflag |= 0;
    tio->ios.c_lflag |= ISIG | ICANON | ECHO | ECHOE | ECHONL;
    

    #define CTRL_KEY(i) \
        ((i) & 037)

    tio->ios.c_cc[VEOF] =   CTRL_KEY('d');
    tio->ios.c_cc[VEOL] =   0000;
    tio->ios.c_cc[VERASE] = 0177;
    tio->ios.c_cc[VINTR] =  CTRL_KEY('c');
    tio->ios.c_cc[VKILL] =  CTRL_KEY('u');
    tio->ios.c_cc[VQUIT] =  CTRL_KEY('|');
    tio->ios.c_cc[VSTART] = CTRL_KEY('q');
    tio->ios.c_cc[VSTOP] =  CTRL_KEY('s');
    tio->ios.c_cc[VSUSP] =  CTRL_KEY('z');

    tio->ios.c_ispeed =
    tio->ios.c_ospeed = B9600;

    tio->winsize.ws_row = 25;
    tio->winsize.ws_col = 80;
    tio->winsize.ws_xpixel = 80 * 8;
    tio->winsize.ws_ypixel = 25 * 16;

    tio->lined = TTYDISC;
    tio->output = 1;
    tio->outlen = 0;

    fifo_init(&tio->in);
    tio->in.async = 1;
    
    
    
    ino->read = tty_read;
    ino->write = tty_write;
    ino->ioctl = tty_ioctl;
    ino->userdata = (void*) tio;
    

    extern int tty_deamon(void*);
    if(sys_clone(tty_deamon, NULL, CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_SIGHAND, NULL) < 0)
        kprintf(ERROR "tty: deamon could not start! Some actions like keystroke's binding will be disabled\n");
    

    sys_symlink("/dev/tty0", "/dev/stdin");
    sys_symlink("/dev/tty0", "/dev/stdout");
    sys_symlink("/dev/tty0", "/dev/stderr");
    
    return 0;
}



int dnit(void) {
    sys_unlink("/dev/stdin");
    sys_unlink("/dev/stdout");
    sys_unlink("/dev/stderr");
    sys_unlink("/dev/tty0");
    return 0;
}
