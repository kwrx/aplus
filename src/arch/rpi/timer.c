#ifdef __rpi__

#include <aplus.h>
#include <arch/rpi/rpi.h>

uint32_t timer_gettime() {
	return 0;
}

uint32_t timer_getticks() {
	return mmio_r32(TIMER_BASE + TIMER_TICK);
}

uint32_t timer_getms() {
	return timer_getticks() / 1000;
}

uint32_t timer_getfreq() {
	return TIMER_FREQ;
}

EXPORT_SYMBOL(timer_gettime);
EXPORT_SYMBOL(timer_getticks);
EXPORT_SYMBOL(timer_getms);
EXPORT_SYMBOL(timer_getfreq);

#endif
