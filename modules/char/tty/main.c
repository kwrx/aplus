#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <libc.h>

#include "tty.h"

MODULE_NAME("char/tty");
MODULE_DEPS("video/fb");
MODULE_AUTHOR("WareX");
MODULE_LICENSE("GPL");


struct termios ios;


int init(void) {
	inode_t* ino;
	if(unlikely((ino = vfs_mkdev("tty", 0, S_IFCHR | 0666)) == NULL))
		return E_ERR;

	memset(&ios, 0, sizeof(ios));
	ios.c_iflag |= 0;
	ios.c_oflag |= 0;
	ios.c_cflag |= 0;
	ios.c_lflag |= ISIG | ICANON | ECHO | ECHOE;
	
	ios.c_cc[VEOF] = 000;
	ios.c_cc[VEOL] = 000;
	ios.c_cc[VERASE] = 0177;
	ios.c_cc[VINTR] = 003;
	ios.c_cc[VKILL] = 025;
	ios.c_cc[VQUIT] = 034;
	ios.c_cc[VMIN] = 0;
	
	
	
	ino->read = tty_read;
	ino->write = tty_write;
	//ino->ioctl = tty_ioctl;
	ino->userdata = (void*) &ios;

	
	sys_symlink("/dev/tty0", "/dev/stdin");
	sys_symlink("/dev/tty0", "/dev/stdout");
	sys_symlink("/dev/tty0", "/dev/stderr");
	
	return E_OK;
}



int dnit(void) {
	return E_OK;
}
