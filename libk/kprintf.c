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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>


static spinlock_t buflock   = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_CPU_OWNER | SPINLOCK_FLAGS_RECURSIVE);
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


    __lock(&buflock, {
        for (int i = 0; buf[i] && !((1 << current_cpu->id) & __atomic_load_n(&accmask, __ATOMIC_CONSUME)); i++) {

            arch_debug_putc(buf[i]);
        }
    });
}


void kprintf_pause(void) {
    spinlock_lock(&buflock);
}

void kprintf_resume(void) {
    spinlock_unlock(&buflock);
}

void kprintf_mask(int mask) {
    __atomic_store_n(&accmask, mask, __ATOMIC_SEQ_CST);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
