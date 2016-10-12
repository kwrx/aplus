#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <libc.h>

MODULE_NAME("i386/uart");
MODULE_DEPS("");
MODULE_AUTHOR("Antonio Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__)
#include <arch/i386/i386.h>

static int COM[] = {
	0x3F8, 0x2F8, 0x3E8, 0x2E8
};


static int uart_ioctl(struct inode* inode, int req, void* buf) {
	errno = ENOSYS;
	return -1;
}

static int uart_read(struct inode* inode, void* buf, size_t size) {

	int idx = '0' - inode->name[4];

	char* bc = (char*) buf;
	size_t i;
	for(i = 0; i < size; i++) {
		while((inb(COM[idx] + 5) & 1) == 0)
			sys_yield();

		*bc++ = inb(COM[idx]);
	}

	return size;
}

static int uart_write(struct inode* inode, void* buf, size_t size) {
	int idx = '0' - inode->name[4];

	char* bc = (char*) buf;
	size_t i;
	for(i = 0; i < size; i++) {
		while((inb(COM[idx] + 5) & 0x20) == 0)
			sys_yield();

		

		outb(COM[idx], *bc++);
	}

	return size;
}

int init(void) {

	int i;
	for(i = 0; i < 4; i++) {
		outb(COM[i] + 1, 0x00);
		outb(COM[i] + 3, 0x80);
		outb(COM[i] + 0, 0x03);
		outb(COM[i] + 1, 0x00);
		outb(COM[i] + 3, 0x03);
		outb(COM[i] + 2, 0xC7);
		outb(COM[i] + 4, 0x0B);

		inode_t* ino = vfs_mkdev("uart", i, S_IFCHR | 0666);
		ino->ioctl = uart_ioctl;
		ino->read = uart_read;
		ino->write = uart_write;
	}
	
	return E_OK;
}


#else

int init(void) {
	return E_ERR;
}

#endif


int dnit(void) {
	return E_OK;
}
