#include <xdev.h>
#include <xdev/ipc.h>
#include <xdev/debug.h>
#include <arch/i386/i386.h>

#if DEBUG
mutex_t mtx_debug = MTX_INIT(MTX_KIND_DEFAULT);


static char __cga_colors[] = {
	7,
	14,
	15,
	12,
	11
};

#define VGA_WIDTH			80
#define VGA_HEIGHT			25

void debug_send(char value, int flags) {

	mutex_lock(&mtx_debug);

	static int p = 0;
	static short* tm = (short*) 0xb8000;
	static int initialized = 0;

	if(!initialized) {
		initialized = 1;

		int x, y;
		for(y = 0; y < VGA_HEIGHT; y++)
			for(x = 0; x < VGA_WIDTH; x++)
				tm[y * VGA_WIDTH + x] = (__cga_colors[0] << 8) | ' ';
				
				
#if CONFIG_SERIAL_DEBUG
		outb(0x3F8 + 1, 0x00);
		outb(0x3F8 + 3, 0x80);
		outb(0x3F8 + 0, 0x03);
		outb(0x3F8 + 1, 0x00);
		outb(0x3F8 + 3, 0x03);
		outb(0x3F8 + 2, 0xC7);
		outb(0x3F8 + 4, 0x0B);
#endif
	}

#if CONFIG_BOCHS
	outb(0xE9, value);
#endif

#if CONFIG_SERIAL_DEBUG
	int i;
	for(i = 0; i < 100000 && ((inb(0x3F8 + 5) & 0x20) == 0); i++)
		;
	outb(0x3F8, value);
#endif

	switch(value) {
		case '\n':
			p += VGA_WIDTH - (p % VGA_WIDTH);
			break;
		case '\v':
			p += VGA_WIDTH;
			break;
		case '\r':
			p -= (p % VGA_WIDTH);
			break;
		case '\t':
			p += 4 - ((p % VGA_WIDTH) % 4);
			break;
		case '\b':
			tm[--p] = (__cga_colors[flags] << 8) | ' ';
			break;
		default:
			tm[p++] = (__cga_colors[flags] << 8) | value;
			break;
	}

	if(p >= (VGA_WIDTH * VGA_HEIGHT)) {
		int x, y;
		for(y = 1; y < VGA_HEIGHT; y++)
			for(x = 0; x < VGA_WIDTH; x++)
				tm[(y - 1) * VGA_WIDTH + x] = tm[y * VGA_WIDTH + x];

		p -= VGA_WIDTH;

		for(x = 0; x < VGA_WIDTH; x++)
			tm[p + x] = (__cga_colors[0] << 8) | ' ';
	}


	outb(0x3D4, 0x0F);
	outb(0x3D5, p & 0xFF);
	outb(0x3D4, 0x0E);
	outb(0x3D5, (p >> 8) & 0xFF);

	mutex_unlock(&mtx_debug);
}

#endif
