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
#include <arch/x86/time.h>

static spinlock_t rtc_lock = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_CPU_OWNER);
static uint8_t rtc_century_register = 0;


static inline uint8_t rtc_read(uint8_t reg) {
    return outb(0x70, reg), inb(0x71);
}

static inline uint8_t bcd_to_binary(uint8_t value) {
    return (value & 0x0F) + ((value >> 4) * 10);
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

uint64_t rtc_gettime(void) {

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

int rtc_init(void) {
    
    acpi_sdt_t* facp = NULL;
    if (acpi_find(&facp, "FACP") == 0) {

        acpi_fadt_t* fadt = NULL;
        if(acpi_is_extended()) {
            fadt = (acpi_fadt_t*)&facp->xtables;
        } else {
            fadt = (acpi_fadt_t*)&facp->tables;
        }

        if (facp->length >= sizeof(acpi_sdt_t) + offsetof(acpi_fadt_t, century) + sizeof(fadt->century)) {
            rtc_century_register = fadt->century;
        } else {
            rtc_century_register = 0;
        }
    }

    return 0;
}



TEST(x86_timer_epoch_test, {
    DEBUG_ASSERT(rtc_to_epoch(1970, 1, 1, 0, 0, 0) == 0);
    DEBUG_ASSERT(rtc_to_epoch(2000, 2, 29, 0, 0, 0) == 951782400);
    DEBUG_ASSERT(rtc_to_epoch(2024, 3, 1, 0, 0, 0) == 1709251200);
});
