#ifdef __i386__
#include <aplus.h>
#include <stdint.h>


#define VIDMEM			0xB8000
#define VIDSIZ			(80 * 25 * 2)
#define VIDX			(80 * 2)
#define VIDY			(25)
#define VIDTAB			4


void arch_debug_putc(uint8_t value) {
#if HAVE_DEBUG_SERIAL
	serial_send(0, value);
#else
	static int cps = 0;
	register uint16_t* vm = (uint16_t*) VIDMEM;

	if(cps == 0)
		memset((void*) VIDMEM, 0, VIDSIZ);

	switch(value) {
		case '\n':
			cps += (VIDX / sizeof(uint16_t)) - (cps % (VIDX / sizeof(uint16_t)));
			break;
		case '\t':
			cps += VIDTAB;
			break;
		case '\v':
			cps += VIDX / sizeof(uint16_t);
			break;
		case '\r':
			cps -= cps % (VIDX / sizeof(uint16_t));
			break;
		case '\b':
			vm[--cps] = (0x7000) | 0x20;
		default:
			vm[cps++] = (0x0700) | (value & 0xFF);
	}
	
 	
	if(cps >= (VIDSIZ / sizeof(uint16_t))) {
		memcpy((void*) VIDMEM, (void*) (VIDMEM + VIDX), VIDSIZ - VIDX);
		memset((void*) (VIDMEM + VIDSIZ - VIDX), 0, VIDX);

		cps -= VIDX / sizeof(uint16_t);
	}
#endif
}

int arch_debug_init(void) {
	memset((void*) VIDMEM, 0, VIDSIZ);

	return 0;
}

#endif
