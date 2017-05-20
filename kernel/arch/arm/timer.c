#include <aplus.h>
#include <libc.h>
#include "arm.h"


long timer_gettimestamp() {
	return 0;
}

long timer_getticks() {
	return mmio_r32(TIMER_BASE + TIMER_TICK);
}

long timer_getms() {
	return timer_getticks() / 1000;
}

long timer_getfreq() {
	return TIMER_FREQ;
}

void timer_delay(long ms) {
	volatile long ticks = timer_getms() + ms;
	while(ticks > timer_getms())
		;
}

EXPORT(timer_gettimestamp);
EXPORT(timer_getticks);
EXPORT(timer_getms);
EXPORT(timer_getfreq);
EXPORT(timer_delay);
