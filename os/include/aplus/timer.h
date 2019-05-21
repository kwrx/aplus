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


#ifndef _APLUS_TIMER_H
#define _APLUS_TIMER_H

#if defined(KERNEL)
#include <aplus.h>
#include <stdint.h>

typedef uint64_t ktime_t;

void arch_timer_delay(ktime_t);
ktime_t arch_timer_getticks(void);
ktime_t arch_timer_getus(void);
ktime_t arch_timer_gettime(void);

#endif
#endif