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
	inline uint8_t RTC(uint8_t x)
		{ outb(0x70, x); return inb(0x71); }
		
	#define BCD2BIN(bcd)					\
		((((bcd) & 0x0F) + ((bcd) / 16) * 10))
		
	#define BCD2BIN2(bcd)					\
		(((((bcd) & 0x0F) + ((bcd & 0x70) / 16) * 10)) | (bcd & 0x80))
		
	static struct tm t;
	t.tm_sec = BCD2BIN(RTC(0));
	t.tm_min = BCD2BIN(RTC(2));
	t.tm_hour = BCD2BIN2(RTC(4));
	t.tm_mday = BCD2BIN(RTC(7));
	t.tm_mon = BCD2BIN(RTC(8));
	t.tm_year = BCD2BIN(RTC(9)) + 100;
	t.tm_wday = 0;
	t.tm_yday = 0;
	t.tm_isdst = 0;
	
	const int m[] =
		{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
		
	long ty = t.tm_year - 70;
	long lp = (ty + 2) / 4;
	long td = 0;
	
	int i;
	for(i = 0; i < t.tm_mon; i++)
		td += m[i];
		
	td += t.tm_mday - 1;
	td = td * (ty * 365) + lp;
	
	return t.tm_sec;
	
	return (long) ((td * 86400) + (t.tm_hour * 3600) +
			(t.tm_min * 60) + t.tm_sec);
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

