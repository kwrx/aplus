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
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




int init(void) {
	tty_read_init();

	inode_t* ino;
	if(unlikely((ino = vfs_mkdev("tty", 0, S_IFCHR | 0666)) == NULL))
		return E_ERR;


	struct tty_context* tio = (struct tty_context*) kmalloc(sizeof(struct tty_context), GFP_KERNEL);
	if(unlikely(!tio)) {
		kprintf(ERROR "tty: no memory left!");
		return E_ERR;
	}

	memset(tio, 0, sizeof(struct tty_context));
	tio->ios.c_iflag |= 0;
	tio->ios.c_oflag |= 0;
	tio->ios.c_cflag |= 0;
	tio->ios.c_lflag |= ISIG | ICANON | ECHO | ECHOE;
	
	tio->ios.c_cc[VEOF] = 000;
	tio->ios.c_cc[VEOL] = 000;
	tio->ios.c_cc[VERASE] = 0177;
	tio->ios.c_cc[VINTR] = 003;
	tio->ios.c_cc[VKILL] = 025;
	tio->ios.c_cc[VQUIT] = 034;
	tio->ios.c_cc[VSTART] = 002;
	tio->ios.c_cc[VSTOP] = 004;
	tio->ios.c_cc[VMIN] = 0;

	tio->ios.c_ispeed =
	tio->ios.c_ospeed = B9600;

	tio->winsize.ws_row = 25;
	tio->winsize.ws_col = 80;
	tio->winsize.ws_xpixel = 80 * 8;
	tio->winsize.ws_ypixel = 25 * 16;

	tio->lined = TTYDISC;
	tio->output = 1;
	tio->outlen = 0;
	
	
	
	ino->read = tty_read;
	ino->write = tty_write;
	ino->ioctl = tty_ioctl;
	ino->userdata = (void*) tio;
	


	sys_symlink("/dev/tty0", "/dev/stdin");
	sys_symlink("/dev/tty0", "/dev/stdout");
	sys_symlink("/dev/tty0", "/dev/stderr");
	
	return E_OK;
}



int dnit(void) {
	return E_OK;
}
