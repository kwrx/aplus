#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"

uint32_t timer_gettime() {
	return 0;
}

uint32_t timer_getticks() {
	return mmio_r32(TIMER_BASE + TIMER_TICK);
}

uint32_t timer_getfreq() {
	return TIMER_FREQ;
}



#endif
