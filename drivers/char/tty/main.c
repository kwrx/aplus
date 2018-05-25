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


    inode_t* ino_outp;
    if(unlikely((ino_outp = vfs_mkdev("tty", 0, S_IFCHR | 0666)) == NULL))
        return -1;

    inode_t* ino_inp;
    if(unlikely((ino_inp = vfs_mkdev("tty", 1, S_IFCHR | 0666)) == NULL))
        return -1;


    struct tty_context* tio = (struct tty_context*) kmalloc(sizeof(struct tty_context), GFP_KERNEL);
    if(unlikely(!tio)) {
        kprintf(ERROR "tty: no memory left!");
        return -1;
    }

    memset(tio, 0, sizeof(struct tty_context));
    tio->ios.c_iflag = TTYDEF_IFLAG;
    tio->ios.c_oflag = TTYDEF_OFLAG;
    tio->ios.c_cflag = TTYDEF_CFLAG;
    tio->ios.c_lflag = TTYDEF_LFLAG;
    

    tio->ios.c_cc[VEOF] = CEOF;
    tio->ios.c_cc[VEOL] = CEOL;
    tio->ios.c_cc[VERASE] = CERASE;
    tio->ios.c_cc[VINTR] = CINTR;
    tio->ios.c_cc[VSTATUS] = CSTATUS;
    tio->ios.c_cc[VKILL] = CKILL;
    tio->ios.c_cc[VMIN] = CMIN;
    tio->ios.c_cc[VQUIT] = CQUIT;
    tio->ios.c_cc[VSUSP] = CSUSP;
    tio->ios.c_cc[VTIME] = CTIME;
    tio->ios.c_cc[VSTART] = CSTART;
    tio->ios.c_cc[VSTOP] = CSTOP;

    tio->ios.c_ispeed =
    tio->ios.c_ospeed = TTYDEF_SPEED;

    tio->winsize.ws_row = 25;
    tio->winsize.ws_col = 80;
    tio->winsize.ws_xpixel = 80 * 8;
    tio->winsize.ws_ypixel = 25 * 16;

    tio->lined = TTYDISC;
    tio->output = 1;
    tio->outlen = 0;

    fifo_init(&tio->in, TTY_BUFSIZ, FIFO_ASYNC);
    fifo_init(&tio->uin, TTY_BUFSIZ, FIFO_ASYNC);
    
    
    ino_outp->read = tty_read;
    ino_outp->write = tty_output_write;
    ino_outp->ioctl = tty_ioctl;
    ino_outp->userdata = (void*) tio;

    ino_inp->read = tty_read;
    ino_inp->write = tty_input_write;
    ino_inp->ioctl = tty_ioctl;
    ino_inp->userdata = (void*) tio;
    

    extern int tty_deamon(void*);
    if(sys_clone(tty_deamon, NULL, CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_SIGHAND, NULL) < 0)
        kprintf(ERROR "tty: deamon could not start! Some actions like keystroke's binding will be disabled\n");
    

    sys_symlink("/dev/tty1", "/dev/stdin");
    sys_symlink("/dev/tty0", "/dev/stdout");
    sys_symlink("/dev/tty0", "/dev/stderr");

    sys_symlink("/dev/tty0", "/dev/tty");   /* fallback */
    return 0;
}



int dnit(void) {
    sys_unlink("/dev/stdin");
    sys_unlink("/dev/stdout");
    sys_unlink("/dev/stderr");
    sys_unlink("/dev/tty");
    sys_unlink("/dev/tty0");
    sys_unlink("/dev/tty1");
    return 0;
}
