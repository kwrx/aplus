#ifdef __i386__

#include <aplus.h>
#include <aplus/spinlock.h>

#include <stdint.h>

#include <arch/i386/i386.h>


static uint16_t serial[] = {
	0x3F8, 0x2F8, 0x3E8, 0x2E8
};

void serial_send(uint8_t port, uint8_t value) {
	while(((inb(serial[port] + 5) & 32) == 0))
		cpu_wait();

	outb(serial[port], value);
}

uint8_t serial_recv(uint8_t port) {
	while((inb(serial[port] + 5) & 1) == 0)
		cpu_wait();
	
	return inb(serial[port]);
}

int serial_init() {
	for(int i = 0; i < 4; i++) {
		outb(serial[i] + 1, 0x00);
		outb(serial[i] + 3, 0x80);
		outb(serial[i] + 0, 0x03);
		outb(serial[i] + 1, 0x00);
		outb(serial[i] + 3, 0x03);
		outb(serial[i] + 2, 0xC7);
		outb(serial[i] + 4, 0x0B);
	}

	return 0;
}


EXPORT_SYMBOL(serial_send);
EXPORT_SYMBOL(serial_recv);

#endif
