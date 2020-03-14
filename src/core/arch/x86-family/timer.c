/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdint.h>
#include <string.h>
#include <time.h>
#include <aplus.h>
#include <aplus/multiboot.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>

#include <hal/cpu.h>
#include <hal/interrupt.h>
#include <hal/vmm.h>
#include <hal/timer.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>


spinlock_t delay_lock;
spinlock_t rtc_lock;

uint64_t clocks_per_ms;


void arch_timer_delay(uint64_t us) {
    
    DEBUG_ASSERT(us > 0);
    DEBUG_ASSERT(us < 1000000);
    DEBUG_ASSERT(clocks_per_ms);


    __lock(&delay_lock, {

#if defined(CONFIG_X86_HAVE_TSC_TIMER)

        uint64_t t0 = arch_timer_getus();

        while(arch_timer_getus() < (t0 + us))
            __builtin_ia32_pause();

#else
        uint32_t sd = 1193180 / (1000000 / us);
        
        outb(0x61, inb(0x61) & ~2);
        outb(0x43, 0x80 | 0x30);
        outb(0x42, sd & 0xFF);
        outb(0x42, (sd >> 8) & 0xFF);
        
        uint8_t cb = inb(0x61);
        outb(0x61, cb & ~1);
        outb(0x61, cb | 1);

        while(!(inb(0x61) & 0x20))
            ;

#endif
            
    });
}


uint64_t arch_timer_getticks(void) {
    return x86_rdtsc();
}

uint64_t arch_timer_getns(void) {
    return x86_rdtsc() / (clocks_per_ms / 1000000);
}

uint64_t arch_timer_getus(void) {
    return x86_rdtsc() / (clocks_per_ms / 1000);
}

uint64_t arch_timer_getms(void) {
    return x86_rdtsc() / (clocks_per_ms);
}

uint64_t arch_timer_getres(void) {
    return clocks_per_ms * 1000;
}


uint64_t arch_timer_gettime(void) {
    
    inline uint8_t RTC(uint8_t x)
        { outb(0x70, x); return inb(0x71); }
        
    #define BCD2BIN(bcd)                        \
        ((((bcd) & 0x0F) + ((bcd) / 16) * 10))
        
    #define BCD2BIN2(bcd)                       \
        (((((bcd) & 0x0F) + ((bcd & 0x70) / 16) * 10)) | (bcd & 0x80))
        

    struct tm t;
    
    __lock(&rtc_lock, {
        t.tm_sec = BCD2BIN(RTC(0));
        t.tm_min = BCD2BIN(RTC(2));
        t.tm_hour = BCD2BIN2(RTC(4));
        t.tm_mday = BCD2BIN(RTC(7));
        t.tm_mon = BCD2BIN(RTC(8));
        t.tm_year = BCD2BIN(RTC(9)) + 2000;
        t.tm_wday = 0;
        t.tm_yday = 0;
        t.tm_isdst = 0;
    });

    
    const int m[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        
    uint64_t ty = t.tm_year - 1970;
    uint64_t lp = (ty + 2) / 4;
    uint64_t td = 0;
    
    int i;
    for(i = 0; i < t.tm_mon - 1; i++)
        td += m[i];
        
    td += t.tm_mday - 1;
    td = td + (ty * 365) + lp;


    return (uint64_t) ((td * 86400) + (t.tm_hour * 3600) +
            (t.tm_min * 60) + t.tm_sec) + 3600;
}

void timer_init(void) {

    spinlock_init(&delay_lock);
    spinlock_init(&rtc_lock);


    // Calibrate TSC Timer
    clocks_per_ms = 0ULL;

    
    // Prepare PIT
    uint32_t sd = 1193180 / 1000;
        
    outb(0x43, 0x80 | 0x30);
    outb(0x42, sd & 0xFF);
    outb(0x42, (sd >> 8) & 0xFF);
        
    


    // Perfom delay
    uint64_t t0 = x86_rdtsc();

    while(!(inb(0x61) & 0x20))
        ;

    uint64_t t1 = x86_rdtsc();
    


    clocks_per_ms = (t1 - t0);
    DEBUG_ASSERT(clocks_per_ms);


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("x86-timer: now(%d) resolution(%d) mHZ(%d)\n", arch_timer_gettime(), arch_timer_getres(), arch_timer_getres() / 1000000);
#endif

}