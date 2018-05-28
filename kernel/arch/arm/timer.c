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
#include <libc.h>
#include "arm.h"


long timer_gettimestamp() {
    return 0;
}

long timer_getticks() {
    return mmio_r32(TIMER_BASE + TIMER_TICK);
}

long timer_getms() {
    return timer_getticks() / 1000;
}

long timer_getfreq() {
    return TIMER_FREQ;
}

void timer_delay(long ms) {
    volatile long ticks = timer_getms() + ms;
    while(ticks > timer_getms())
        ;
}

EXPORT(timer_gettimestamp);
EXPORT(timer_getticks);
EXPORT(timer_getms);
EXPORT(timer_getfreq);
EXPORT(timer_delay);
