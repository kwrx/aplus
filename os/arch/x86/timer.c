/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/timer.h>
#include <aplus/smp.h>
#include <arch/x86/mm.h>
#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/cpu.h>
#include <arch/x86/smp.h>
#include <string.h>

spinlock_t delay_lock;


void arch_timer_delay(ktime_t us) {
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

ktime_t arch_timer_getticks(void) {
    return current_cpu->ticks;
}

ktime_t arch_timer_getus(void) {
    return current_cpu->ticks;
}

ktime_t arch_timer_gettime(void) {
    return 0;
}

void timer_init(void) {
    spinlock_init(&delay_lock);
}