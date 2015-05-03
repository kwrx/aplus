#ifdef __rpi__
#include <aplus.h>
#include <stdint.h>


void arch_debug_putc(uint8_t val) {
#if HAVE_DEBUG_SERIAL
	serial_send(0, ch);
#endif
}

int arch_debug_init(void) {
	return 0;
}

#endif
