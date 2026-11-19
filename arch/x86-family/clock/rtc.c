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

static inline void rtc_write(uint8_t reg, uint8_t value) {
    outb(0x70, reg);
    outb(0x71, value);
}

static inline uint8_t bcd_to_binary(uint8_t value) {
    return (value & 0x0F) + ((value >> 4) * 10);
}

static inline uint8_t binary_to_bcd(uint8_t value) {
    return (uint8_t)(((value / 10) << 4) | (value % 10));
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

static void rtc_from_epoch(uint64_t epoch, uint64_t* year, rtc_time_t* time, uint8_t* weekday) {

    const uint64_t days = epoch / 86400;
    const uint64_t secs = epoch % 86400;

    time->hour   = (uint8_t)(secs / 3600);
    time->minute = (uint8_t)((secs / 60) % 60);
    time->second = (uint8_t)(secs % 60);

    *weekday = (uint8_t)(((days + 4) % 7) + 1);

    const uint64_t z   = days + 719468;
    const uint64_t era = z / 146097;
    const uint64_t doe = z - era * 146097;
    const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint64_t mp  = (5 * doy + 2) / 153;

    time->day   = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
    time->month = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);

    *year = yoe + era * 400 + (time->month <= 2);
}

void rtc_settime(uint64_t epoch) {

    rtc_time_t time;
    uint64_t year;
    uint8_t weekday;

    rtc_from_epoch(epoch, &year, &time, &weekday);

    if (year < 2000 || year > 9999 || (!rtc_century_register && year > 2099)) {
        return;
    }

    time.year    = (uint8_t)(year % 100);
    time.century = (uint8_t)(year / 100);

    scoped_lock(&rtc_lock) {

        const uint8_t status = rtc_read(0x0B);

        uint8_t hour = time.hour;
        uint8_t pm   = 0;

        if ((status & 0x02) == 0) {
            pm   = hour >= 12 ? 0x80 : 0;
            hour = hour % 12 ? hour % 12 : 12;
        }

        if ((status & 0x04) == 0) {
            time.second  = binary_to_bcd(time.second);
            time.minute  = binary_to_bcd(time.minute);
            hour         = binary_to_bcd(hour);
            time.day     = binary_to_bcd(time.day);
            time.month   = binary_to_bcd(time.month);
            time.year    = binary_to_bcd(time.year);
            time.century = binary_to_bcd(time.century);
            weekday      = binary_to_bcd(weekday);
        }

        rtc_write(0x0B, status | 0x80);

        rtc_write(0x00, time.second);
        rtc_write(0x02, time.minute);
        rtc_write(0x04, hour | pm);
        rtc_write(0x06, weekday);
        rtc_write(0x07, time.day);
        rtc_write(0x08, time.month);
        rtc_write(0x09, time.year);

        if (rtc_century_register) {
            rtc_write(rtc_century_register, time.century);
        }

        rtc_write(0x0B, status & ~0x80);
    };
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

    rtc_time_t time;
    uint64_t year;
    uint8_t weekday;

    rtc_from_epoch(951782400, &year, &time, &weekday);
    DEBUG_ASSERT(year == 2000 && time.month == 2 && time.day == 29 && weekday == 3);

    rtc_from_epoch(1709251200 + 3723, &year, &time, &weekday);
    DEBUG_ASSERT(year == 2024 && time.month == 3 && time.day == 1 && time.hour == 1 && time.minute == 2 && time.second == 3);
});
