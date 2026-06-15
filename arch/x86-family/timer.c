/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/acpi.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>



#define LOOP_SANITY_CHECK 32


#define HPET_TICK 1000000000000000

#define HPET_GENERAL_GCID    hpet_address + 0x00
#define HPET_GENERAL_CR      hpet_address + 0x10
#define HPET_GENERAL_ISR     hpet_address + 0x20
#define HPET_GENERAL_COUNTER hpet_address + 0xF0

static spinlock_t rtc_lock;

static uint64_t tsc_frequency        = 1;
static uint64_t hpet_frequency       = 1;
static uint64_t hpet_period          = 1;
static uintptr_t hpet_address        = 0;
static uint8_t rtc_century_register = 0;


typedef struct {

    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;

} rtc_time_t;


static inline uint8_t rtc_read(uint8_t reg) {
    return outb(0x70, reg), inb(0x71);
}

static inline uint8_t bcd_to_binary(uint8_t value) {
    return (value & 0x0F) + ((value >> 4) * 10);
}

static uint64_t timer_scale(uint64_t ticks, uint64_t scale, uint64_t frequency) {

    DEBUG_ASSERT(scale);
    DEBUG_ASSERT(frequency);
    DEBUG_ASSERT(frequency <= UINT64_MAX / scale);

    return ((ticks / frequency) * scale) + (((ticks % frequency) * scale) / frequency);
}

static int rtc_is_leap_year(uint64_t year) {
    return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
}

static uint64_t rtc_to_epoch(uint64_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {

    static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    PANIC_ASSERT(year >= 1970);
    PANIC_ASSERT(month >= 1 && month <= 12);
    PANIC_ASSERT(day >= 1 && day <= days_in_month[month - 1] + ((month == 2) && rtc_is_leap_year(year)));
    PANIC_ASSERT(hour < 24);
    PANIC_ASSERT(minute < 60);
    PANIC_ASSERT(second < 60);


    uint64_t previous_year = year - 1;
    uint64_t days = ((year - 1970) * 365) + (previous_year / 4) - (previous_year / 100) + (previous_year / 400) - 477;

    for (uint8_t i = 1; i < month; i++)
        days += days_in_month[i - 1];

    if (month > 2 && rtc_is_leap_year(year))
        days++;

    days += day - 1;

    return (days * 86400) + (hour * 3600) + (minute * 60) + second;
}

static void rtc_read_time(rtc_time_t* time) {

    DEBUG_ASSERT(time);

    time->second  = rtc_read(0x00);
    time->minute  = rtc_read(0x02);
    time->hour    = rtc_read(0x04);
    time->day     = rtc_read(0x07);
    time->month   = rtc_read(0x08);
    time->year    = rtc_read(0x09);
    time->century = rtc_century_register ? rtc_read(rtc_century_register) : 0;
}



void arch_timer_delay(uint64_t us) {

    DEBUG_ASSERT(us > 0);
    DEBUG_ASSERT(us < 100000000); // 10sec max


    uint64_t start = arch_timer_generic_getus();

    while ((arch_timer_generic_getus() - start) < us)
        __builtin_ia32_pause();
}


uint64_t arch_timer_gettime(void) {

    rtc_time_t first;
    rtc_time_t second;
    uint8_t status;

    scoped_lock(&rtc_lock) {

        do {
            while (rtc_read(0x0A) & 0x80)
                __builtin_ia32_pause();

            rtc_read_time(&first);

            while (rtc_read(0x0A) & 0x80)
                __builtin_ia32_pause();

            rtc_read_time(&second);

        } while (memcmp(&first, &second, sizeof(first)) != 0);

        status = rtc_read(0x0B);
    };


    uint8_t pm = second.hour & 0x80;
    second.hour &= 0x7F;

    if ((status & 0x04) == 0) {
        second.second  = bcd_to_binary(second.second);
        second.minute  = bcd_to_binary(second.minute);
        second.hour    = bcd_to_binary(second.hour);
        second.day     = bcd_to_binary(second.day);
        second.month   = bcd_to_binary(second.month);
        second.year    = bcd_to_binary(second.year);
        second.century = bcd_to_binary(second.century);
    }

    if ((status & 0x02) == 0)
        second.hour = (second.hour % 12) + (pm ? 12 : 0);

    uint64_t year = second.century ? ((uint64_t)second.century * 100) + second.year : 2000 + second.year;

    return rtc_to_epoch(year, second.month, second.day, second.hour, second.minute, second.second);
}



uint64_t arch_timer_percpu_getticks(void) {
    return x86_rdtsc();
}

uint64_t arch_timer_percpu_getns(void) {
    return timer_scale(x86_rdtsc(), 1000000, tsc_frequency);
}

uint64_t arch_timer_percpu_getus(void) {
    return timer_scale(x86_rdtsc(), 1000, tsc_frequency);
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
    return timer_scale(mmio_r64(HPET_GENERAL_COUNTER), hpet_period, 1000000);
}

uint64_t arch_timer_generic_getus(void) {
    return timer_scale(mmio_r64(HPET_GENERAL_COUNTER), hpet_period, 1000000000);
}

uint64_t arch_timer_generic_getms(void) {
    return timer_scale(mmio_r64(HPET_GENERAL_COUNTER), hpet_period, 1000000000000);
}

uint64_t arch_timer_generic_getres(void) {
    return hpet_frequency;
}



void timer_init(void) {

    spinlock_init_with_flags(&rtc_lock, SPINLOCK_FLAGS_CPU_OWNER);

    acpi_sdt_t* facp = NULL;
    if (acpi_find(&facp, "FACP") == 0) {
        acpi_fadt_t* fadt = acpi_is_extended() ? (acpi_fadt_t*)&facp->xtables : (acpi_fadt_t*)&facp->tables;

        if (facp->length >= sizeof(acpi_sdt_t) + offsetof(acpi_fadt_t, century) + sizeof(fadt->century))
            rtc_century_register = fadt->century;
    }


    acpi_sdt_t* sdt = NULL;

    if (acpi_find(&sdt, "HPET") != 0) {

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

        if (acpi_is_extended())
            hpet = (acpi_hpet_t*)&sdt->xtables;
        else
            hpet = (acpi_hpet_t*)&sdt->tables;

        DEBUG_ASSERT(hpet);
        DEBUG_ASSERT(hpet->address.type == 0);
        DEBUG_ASSERT(hpet->address.address != 0);


#if DEBUG_LEVEL_INFO
        kprintf("hpet: rev(%d) count(%d) counter(%d) nr(%d) mintick(%d) address(0x%lX)\n", hpet->hardware_rev_id, hpet->comparator_count, hpet->counter_size, hpet->hpet_number, hpet->minimum_tick, hpet->address.address);
#endif


        arch_vmm_map(&core->bsp.address_space, hpet->address.address, hpet->address.address, PML1_PAGESIZE,

                     ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED);


        hpet_address = hpet->address.address;


        uint64_t capabilities = mmio_r64(HPET_GENERAL_GCID);
        uint16_t timers       = ((capabilities >> 8) & 0x1F) + 1;
        uint64_t period       = capabilities >> 32;


        PANIC_ASSERT(capabilities & (1ULL << 13));
        PANIC_ASSERT(period);


        hpet_period    = period;
        hpet_frequency = HPET_TICK / hpet_period;
        PANIC_ASSERT(hpet_frequency);


#if 0
        // uint16_t i;
        // for(i = 0; i < timers - 1; i++)
        //     mmio_w64(mmio_w64()) // TODO: Initialize HPET Timers
#else
        (void)timers;
#endif

        mmio_w64(HPET_GENERAL_ISR, mmio_r64(HPET_GENERAL_ISR));
        mmio_w64(HPET_GENERAL_CR, mmio_r64(HPET_GENERAL_CR) | 1);


#if DEBUG_LEVEL_INFO
        kprintf("hpet: started! mHZ(%ld) period(%ld) timers(%d)\n", hpet_frequency / 1000000, period, timers);
#endif



        uint64_t d, s, e;


        tsc_frequency = 0ULL;

        for (int j = 0; j < LOOP_SANITY_CHECK; j++) {

            d = mmio_r64(HPET_GENERAL_COUNTER);


            s = x86_rdtsc();

            while ((mmio_r64(HPET_GENERAL_COUNTER) - d) < (hpet_frequency / 1000))
                __builtin_ia32_pause();

            e = x86_rdtsc();


            tsc_frequency += (e - s);
        }

        tsc_frequency /= LOOP_SANITY_CHECK;
    }



    DEBUG_ASSERT(tsc_frequency);
    DEBUG_ASSERT(hpet_frequency);
    DEBUG_ASSERT(hpet_address);


#if DEBUG_LEVEL_INFO
    kprintf("x86-timer: now(%ld) percpu[mHZ(%lld)] generic[mHZ(%lld)]\n", arch_timer_gettime(), arch_timer_percpu_getres() / 1000000ULL, arch_timer_generic_getres() / 1000000ULL);
#endif
}



TEST(x86_timer_delay_test, {
    uint64_t now = arch_timer_generic_getus();

    arch_timer_delay(100);

    uint64_t then = arch_timer_generic_getus();

    DEBUG_ASSERT(then - now >= 100);
});

TEST(x86_timer_scale_test, {
    DEBUG_ASSERT(timer_scale(3, 10, 4) == 7);
    DEBUG_ASSERT(timer_scale(123456789012345ULL, 1000000000, 100000000) == 1234567890123450ULL);
    DEBUG_ASSERT(timer_scale(UINT64_MAX / 1000000000, 1000000000, 1000000000) == UINT64_MAX / 1000000000);
});

TEST(x86_timer_epoch_test, {
    DEBUG_ASSERT(rtc_to_epoch(1970, 1, 1, 0, 0, 0) == 0);
    DEBUG_ASSERT(rtc_to_epoch(2000, 2, 29, 0, 0, 0) == 951782400);
    DEBUG_ASSERT(rtc_to_epoch(2024, 3, 1, 0, 0, 0) == 1709251200);
});
