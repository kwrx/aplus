/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>


static atomic_int buflock   = 0;
static volatile int accmask = 0;


/*!
 * @brief Print formatted output to the debugger.
 */
__nosanitize("undefined") void kprintf(const char* fmt, ...) {

    char buf[CONFIG_BUFSIZ] = {0};

    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), fmt, v);
    va_end(v);


    long flags = kprintf_lock();
    for (int i = 0; buf[i] && !((1 << current_cpu->id) & atomic_load_explicit(&accmask, memory_order_consume)); i++) {
        arch_debug_putc(buf[i]);
    }
    kprintf_unlock(flags);
}


long kprintf_lock(void) {
    long flags = arch_intr_disable();
    while (atomic_exchange_explicit(&buflock, 1, memory_order_acquire)) {
        __cpu_pause();
    }
    return flags;
}

void kprintf_unlock(long flags) {
    atomic_store_explicit(&buflock, 0, memory_order_release);
    arch_intr_restore(flags);
}

void kprintf_mask(int mask) {
    atomic_store(&accmask, mask);
    atomic_thread_fence(memory_order_release);
}
