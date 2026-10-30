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
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>

#include <arch/x86/acpi.h>
#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/time.h>

#define TSC_CALIBRATE_MS        50
#define TSC_CALIBRATE_MAX_READS 10000000
#define TSC_CPUID_LEAF          0x15


static uint64_t tsc_frequency = 0;


static uint64_t tsc_cpuid_frequency(void) {

    long ax, bx, cx, dx;

    x86_cpuid(0, &ax, &bx, &cx, &dx);

    if (ax < TSC_CPUID_LEAF)
        return 0;

    x86_cpuid(TSC_CPUID_LEAF, &ax, &bx, &cx, &dx);

    if (!ax || !bx || !cx)
        return 0;

    return ((uint64_t)cx * (uint64_t)bx) / (uint64_t)ax;
}

int tsc_init(void) {

    if (!boot_cpu_has(X86_FEATURE_TSC))
        return -ENOSYS;

    tsc_frequency = tsc_cpuid_frequency();
    return 0;
}

int tsc_calibrate(uint64_t (*getticks)(void), uint64_t (*getres)(void)) {

    DEBUG_ASSERT(getticks);
    DEBUG_ASSERT(getres);
    DEBUG_ASSERT(getres());

    uint64_t window = (getres() * TSC_CALIBRATE_MS) / 1000;

    uint64_t r0 = getticks();
    uint64_t t0 = x86_rdtsc();

    uint64_t r1 = r0;
    for (size_t i = 0; i < TSC_CALIBRATE_MAX_READS && (r1 - r0) < window; i++)
        r1 = getticks();

    uint64_t t1 = x86_rdtsc();

    if ((r1 - r0) < window)
        return -EINVAL;

    tsc_frequency = ((t1 - t0) * getres()) / (r1 - r0);

#if DEBUG_LEVEL_TRACE
    kprintf("tsc: calibrated over %d ms: mHZ(%ld)\n", TSC_CALIBRATE_MS, tsc_frequency / 1000000);
#endif

    return tsc_frequency ? 0 : -EINVAL;
}

uint64_t tsc_getticks(void) {
    return x86_rdtsc();
}

uint64_t tsc_getres(void) {
    return tsc_frequency;
}
