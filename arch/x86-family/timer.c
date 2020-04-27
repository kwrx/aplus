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
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/acpi.h>




#define LOOP_SANITY_CHECK               32


#define HPET_TICK                       1000000000000000

#define HPET_GENERAL_GCID               hpet_address + 0x00
#define HPET_GENERAL_CR                 hpet_address + 0x10
#define HPET_GENERAL_ISR                hpet_address + 0x20
#define HPET_GENERAL_COUNTER            hpet_address + 0xF0

#define HPET_TIMER_CCR(i)               hpet_address + 0x100 + (0x20 * i)
#define HPET_TIMER_CVR(i)               hpet_address + 0x108 + (0x20 * i)
#define HPET_TIMER_FSB(i)               hpet_address + 0x110 + (0x20 * i)





static spinlock_t delay_lock;
static spinlock_t rtc_lock;

static uint64_t tsc_frequency;
static uint64_t hpet_frequency;
static uintptr_t hpet_address;





void arch_timer_delay(uint64_t us) {
    
    DEBUG_ASSERT(us > 0);
    DEBUG_ASSERT(us < 100000000);   // 10sec max


    uint64_t t0 = arch_timer_generic_getus();

    while(arch_timer_generic_getus() < (t0 + us))
        __builtin_ia32_pause();
        

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
        t.tm_sec  = BCD2BIN(RTC(0));
        t.tm_min  = BCD2BIN(RTC(2));
        t.tm_hour = BCD2BIN2(RTC(4));
        t.tm_mday = BCD2BIN(RTC(7));
        t.tm_mon  = BCD2BIN(RTC(8));
        t.tm_year = BCD2BIN(RTC(9)) + 2000;
        t.tm_wday = 0;
        t.tm_yday = 0;
        t.tm_isdst = 0;
    });


    static int m[] =
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



uint64_t arch_timer_percpu_getticks(void) {
    return x86_rdtsc();
}

uint64_t arch_timer_percpu_getns(void) {
    return x86_rdtsc() * 1000000 / tsc_frequency;
}

uint64_t arch_timer_percpu_getus(void) {
    return x86_rdtsc() * 1000 / tsc_frequency;
}

uint64_t arch_timer_percpu_getms(void) {
    return x86_rdtsc() / tsc_frequency;
}

uint64_t arch_timer_percpu_getres(void) {
    return tsc_frequency * 1000;
}



uint64_t arch_timer_generic_getticks(void) {
    return mmio_r64(HPET_GENERAL_COUNTER);
}

uint64_t arch_timer_generic_getns(void) {
    return mmio_r64(HPET_GENERAL_COUNTER) * 1000000000 / hpet_frequency;
}

uint64_t arch_timer_generic_getus(void) {
    return mmio_r64(HPET_GENERAL_COUNTER) * 1000000 / hpet_frequency;
}

uint64_t arch_timer_generic_getms(void) {
    return mmio_r64(HPET_GENERAL_COUNTER) * 1000 / hpet_frequency;
}

uint64_t arch_timer_generic_getres(void) {
    return hpet_frequency;
}



void timer_init(void) {

    spinlock_init_with_flags(&delay_lock, SPINLOCK_FLAGS_CPU_OWNER);
    spinlock_init_with_flags(&rtc_lock, SPINLOCK_FLAGS_CPU_OWNER);



    acpi_sdt_t* sdt;
    if(acpi_find(&sdt, "HPET") != 0) {

        kpanicf("x86-timer: PANIC! HPET not found in ACPI tables, required!\n");



        // *
        // * PIT Fallback (Disabled)
        // *

        // // uint64_t d, s, e;


        // // tsc_frequency = 0ULL;

        // // for(int j = 0; j < LOOP_SANITY_CHECK; j++) {


        // //     d = 1193180 / 1000;
                
        // //     outb(0x43, (0x80 | 0x30));
        // //     outb(0x42, (d & 0xFF));
        // //     outb(0x42, (d >> 8) & 0xFF);


        // //     s = x86_rdtsc();

        // //         while(!(inb(0x61) & 0x20))
        // //             ;

        // //     e = x86_rdtsc();
        
        // //     tsc_frequency += (e - s);

        // // }

        // // tsc_frequency /= LOOP_SANITY_CHECK;




    } else {

        acpi_hpet_t* hpet;

        if(acpi_is_extended())
            hpet = (acpi_hpet_t*) &sdt->xtables;
        else
            hpet = (acpi_hpet_t*) &sdt->tables;

        DEBUG_ASSERT(hpet);
        DEBUG_ASSERT(hpet->address.type == 0);
        DEBUG_ASSERT(hpet->address.address != 0);


#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("hpet: rev(%d) count(%d) counter(%d) nr(%d) mintick(%d) address(%p)\n",
            hpet->hardware_rev_id,
            hpet->comparator_count,
            hpet->counter_size,
            hpet->hpet_number,
            hpet->minimum_tick,
            hpet->address.address
        );
#endif


        arch_vmm_map (
            &core->bsp.address_space,
            hpet->address.address,
            hpet->address.address,
            PML1_PAGESIZE,
            
            ARCH_VMM_MAP_RDWR       |
            ARCH_VMM_MAP_UNCACHED   |
            ARCH_VMM_MAP_NOEXEC     |
            ARCH_VMM_MAP_FIXED
        );


        hpet_address = hpet->address.address;


        uint16_t timers = (mmio_r64(HPET_GENERAL_GCID) >>  7) & 0xF;
        uint64_t period = (mmio_r64(HPET_GENERAL_GCID) >> 32) & 0xFFFFFFFF;
        uint64_t freq   = (HPET_TICK / period);


        DEBUG_ASSERT(period);
        DEBUG_ASSERT(freq);
        DEBUG_ASSERT(timers);
        DEBUG_ASSERT(timers < 32);


        hpet_frequency = freq;


        // uint16_t i;
        // for(i = 0; i < timers - 1; i++)
        //     mmio_w64(mmio_w64()) // TODO: Initialize HPET Timers


        mmio_w64(HPET_GENERAL_ISR, mmio_r64(HPET_GENERAL_ISR) & 0xFFFFFFFF00000000);
        mmio_w64(HPET_GENERAL_CR, mmio_r64(HPET_GENERAL_CR) | 1);


#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("hpet: started! mHZ(%d) period(%d) timers(%d)\n", freq / 1000000, period, timers);
#endif




        uint64_t d, s, e;


        tsc_frequency = 0ULL;

        for(int j = 0; j < LOOP_SANITY_CHECK; j++) {

            d = mmio_r64(HPET_GENERAL_COUNTER) + (freq / 1000);


            s = x86_rdtsc();

                while(mmio_r64(HPET_GENERAL_COUNTER) < d)
                    ;

            e = x86_rdtsc();


            tsc_frequency += (e - s);

        }

        tsc_frequency /= LOOP_SANITY_CHECK;


    }




    DEBUG_ASSERT(tsc_frequency);
    DEBUG_ASSERT(hpet_frequency);
    DEBUG_ASSERT(hpet_address);


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("x86-timer: now(%d) percpu[mHZ(%d)] generic[mHZ(%d)]\n", arch_timer_gettime(),
                                                                     arch_timer_percpu_getres()  / 1000000ULL,
                                                                     arch_timer_generic_getres() / 1000000ULL);
#endif

}