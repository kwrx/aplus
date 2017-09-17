#include <aplus.h>
#include <aplus/intr.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include <arch/i386/i386.h>


static ktime_t ticks = 0;
static ktime_t seconds = 0;
static ktime_t days = 0;
static ktime_t jiffies = 0;
static ktime_t cycles = 0;


static void timer_handler(void* context) {
    ticks += 1;
    cycles = rdtsc();
            
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
    ktime_t f = 1193180 / TIMER_FREQ;
    
    outb(0x43, 0x36);
    outb(0x40, (uint8_t) (f & 0xFF));
    outb(0x40, (uint8_t) ((f >> 8) & 0xFF));

    irq_enable(0, timer_handler);


    INTR_ON;
    timer_delay(1);


    int i;
    for(i = 0; i < 100; i++) {
        uint64_t t0, t1;
        
        t0 = rdtsc();
        timer_delay(1);
        t1 = rdtsc();

        if(likely(jiffies))
            jiffies = (jiffies + (t1 - t0)) >> 1;
        else
            jiffies = t1 - t0;
    }



    INTR_OFF;
    return E_OK;
}

ktime_t timer_gettimestamp() {
    inline uint8_t RTC(uint8_t x)
        { outb(0x70, x); return inb(0x71); }
        
    #define BCD2BIN(bcd)                        \
        ((((bcd) & 0x0F) + ((bcd) / 16) * 10))
        
    #define BCD2BIN2(bcd)                       \
        (((((bcd) & 0x0F) + ((bcd & 0x70) / 16) * 10)) | (bcd & 0x80))
        
    static struct tm t;
    t.tm_sec = BCD2BIN(RTC(0));
    t.tm_min = BCD2BIN(RTC(2));
    t.tm_hour = BCD2BIN2(RTC(4));
    t.tm_mday = BCD2BIN(RTC(7));
    t.tm_mon = BCD2BIN(RTC(8));
    t.tm_year = BCD2BIN(RTC(9)) + 2000;
    t.tm_wday = 0;
    t.tm_yday = 0;
    t.tm_isdst = 0;

    
    const int m[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        
    ktime_t ty = t.tm_year - 1970;
    ktime_t lp = (ty + 2) / 4;
    ktime_t td = 0;
    
    int i;
    for(i = 0; i < t.tm_mon - 1; i++)
        td += m[i];
        
    td += t.tm_mday - 1;
    td = td + (ty * 365) + lp;

    return (ktime_t) ((td * 86400) + (t.tm_hour * 3600) +
            (t.tm_min * 60) + t.tm_sec);
}


ktime_t timer_getticks() {
    return ((days * 86400) * TIMER_FREQ) + (seconds * TIMER_FREQ) + ticks;
}

ktime_t timer_getms() {
    return timer_getticks();
}

ktime_t timer_getus() {
    long double b = (long double) jiffies;
    long double a = ((long double) (rdtsc() - cycles) / (b / 1000.0));
    
    return (timer_getticks() * 1000) + ((ktime_t) a % 1000);
}

ktime_t timer_getfreq() {
    return TIMER_FREQ;
}

__optimize(0)
void timer_delay(ktime_t ms) {
    volatile ktime_t tk = timer_getms() + ms;
    while(tk > timer_getms())
        __asm__ __volatile__ ("pause;");
}



EXPORT(timer_gettimestamp);
EXPORT(timer_getticks);
EXPORT(timer_getms);
EXPORT(timer_getus);
EXPORT(timer_getfreq);
EXPORT(timer_delay);

