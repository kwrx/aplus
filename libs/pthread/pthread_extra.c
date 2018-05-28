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


#include "pthread_internal.h"

int pthread_equal(pthread_t t1, pthread_t t2) {
    return (t1 == t2);
}

int	pthread_getcpuclockid (pthread_t thread, clockid_t *clock_id) {
    if(clock_id)
        *clock_id = CLOCK_MONOTONIC;

    return 0;
}


int	pthread_setconcurrency (int new_level) {
    return 0;
}

int	pthread_getconcurrency (void) {
    return 0;
}

void pthread_yield (void) {
    sched_yield();
}