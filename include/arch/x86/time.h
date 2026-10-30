/*
 * Author(s):
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

#ifndef _APLUS_X86_TIME_H
#define _APLUS_X86_TIME_H

#ifndef __ASSEMBLY__

    #include <aplus.h>
    #include <aplus/debug.h>

    typedef struct {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;
        uint8_t month;
        uint8_t year;
        uint8_t century;
    } rtc_time_t;

    int rtc_init(void);
    uint64_t rtc_gettime(void);

    int hpet_init(void);
    uint64_t hpet_getticks(void);
    uint64_t hpet_getres(void);

    int pmtmr_init(void);
    uint64_t pmtmr_getticks(void);
    uint64_t pmtmr_getres(void);

    int pit_init(void);
    uint64_t pit_getticks(void);
    uint64_t pit_getres(void);

    int tsc_init(void);
    uint64_t tsc_getticks(void);
    uint64_t tsc_getres(void);
    int tsc_calibrate(uint64_t (*getticks)(void), uint64_t (*getres)(void));

#endif

#endif