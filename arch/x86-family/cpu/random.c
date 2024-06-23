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

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/acpi.h>
#include <arch/x86/apic.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>


static uint64_t (*__random_hook)(void) = NULL;


#if defined(CONFIG_X86_ENABLE_RDRAND)
static uint64_t __rdrand(void) {

    unsigned long long int r;

    do {
        __cpu_pause();
    } while (!__builtin_ia32_rdrand64_step(&r));

    return r;
}
#endif

static uint64_t __dummy(void) {
    return ((((uint64_t)rand()) << 32LL) | rand());
}



uint64_t arch_random(void) {

    DEBUG_ASSERT(__random_hook);

    return __random_hook();
}


void random_init() {

    srand(arch_timer_gettime());

#if defined(CONFIG_X86_ENABLE_RDRAND)
    if (cpu_has(current_cpu->id, X86_FEATURE_RDRAND)) {
        __random_hook = __rdrand;
    } else
#endif
    {
        __random_hook = __dummy;
    }
}


TEST(x86_random_test, {
    uint64_t a = arch_random();
    uint64_t b = arch_random();

    DEBUG_ASSERT(a != b);
});
