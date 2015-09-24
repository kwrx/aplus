#include <xdev.h>
#include <xdev/intr.h>
#include <xdev/debug.h>
#include <xdev/task.h>
#include <libc.h>

#include <arch/i386/i386.h>


static long ticks = 0;
static long seconds = 0;
static long days = 0;


static void timer_handler(void* context) {
	ticks += 1;
			
	if(unlikely(ticks >= TIMER_FREQ)) {
		ticks = 0;
		seconds += 1;
	}

	if(unlikely(seconds >= 86400)) {
		seconds = 0;
		days += 1;
	}

	schedule();
}

int timer_init() {
	long f = 1193180 / TIMER_FREQ;
	
	outb(0x43, 0x36);
	outb(0x40, (uint8_t) (f & 0xFF));
	outb(0x40, (uint8_t) ((f >> 8) & 0xFF));

	irq_enable(0, timer_handler);
	return E_OK;
}

long timer_gettime() {
	return 0;
}

long timer_getticks() {
	return ((days * 86400) * TIMER_FREQ) + (seconds * TIMER_FREQ) + ticks;
}

long timer_getms() {
	return timer_getticks();
}

long timer_getfreq() {
	return TIMER_FREQ;
}

__optimize(0)
void timer_delay(long ms) {
	volatile long tk = timer_getms() + ms;
	while(tk > timer_getms())
		__asm__ __volatile__ ("pause;");
}

EXPORT(timer_gettime);
EXPORT(timer_getticks);
EXPORT(timer_getms);
EXPORT(timer_getfreq);
EXPORT(timer_delay);

