#include <stdint.h>
#include <aplus.h>
#include <aplus/int86.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>
#include <aplus/ioctl.h>


static uint16_t ports[] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8 };


void serial_init() {
	for(int i = 0; i < sizeof(ports); i++) {
		outb(ports[i] + 1, 0x00);
		outb(ports[i] + 3, 0x80);
		outb(ports[i] + 0, 0x03);
		outb(ports[i] + 1, 0x00);
		outb(ports[i] + 3, 0x03);
		outb(ports[i] + 2, 0xC7);
		outb(ports[i] + 4, 0x0B);
	}
}


int serial_recv(int com) {
	while((inb(ports[com] + 5) & 1) == 0);
	return inb(ports[com]);
}

int serial_send(int com, char v) {
	while((inb(ports[com] + 5) & 0x20) == 0);
	outb(ports[com], v);
	
	return 0;
}