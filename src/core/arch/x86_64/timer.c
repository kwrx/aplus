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
#include <aplus/core/base.h>
#include <aplus/core/multiboot.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>
#include <aplus/core/ipc.h>
#include <aplus/core/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>


spinlock_t delay_lock;
spinlock_t rtc_lock;


void arch_timer_delay(uint64_t us) {
    
    DEBUG_ASSERT(us > 0);
    DEBUG_ASSERT(us < 1000000);

    __lock(&delay_lock, {

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

    });
}


uint64_t arch_timer_getticks(void) {
    return (uint64_t) (current_cpu->ticks.tv_sec * CLOCKS_PER_SEC) +
           (uint64_t) (current_cpu->ticks.tv_nsec / (1000000000 / CLOCKS_PER_SEC));
}

uint64_t arch_timer_getus(void) {
    return (uint64_t) (current_cpu->ticks.tv_sec * CLOCKS_PER_SEC) +
           (uint64_t) (current_cpu->ticks.tv_nsec / 1000);
}

uint64_t arch_timer_getms(void) {
    return (uint64_t) (current_cpu->ticks.tv_sec * CLOCKS_PER_SEC) +
           (uint64_t) (current_cpu->ticks.tv_nsec / 1000000);
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

    kprintf("x86-timer: now(%d) CLOCKS_PER_SEC(%d)\n", arch_timer_gettime(), CLOCKS_PER_SEC);

}