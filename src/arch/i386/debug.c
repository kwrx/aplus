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
	12
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
	}

#if CONFIG_BOCHS
	outb(0xE9, value);
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
			p += 4;
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

	mutex_unlock(&mtx_debug);
}

#endif
