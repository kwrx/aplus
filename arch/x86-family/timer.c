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
#include <arch/x86/intr.h>



/**
 * @brief Reads no clock, before timer_init() has chosen one.
 *
 * @return Always 0.
 */
static uint64_t timer_null_getticks(void) {
    return 0;
}

/**
 * @brief Reports a placeholder frequency, before timer_init() has chosen a clock.
 *
 * @return Always 1, so that scaling yields 0 rather than dividing by zero.
 */
static uint64_t timer_null_getres(void) {
    return 1;
}


static uint64_t realtime_base_ns = 0;
static uint64_t generic_origin   = 0;

static uint64_t (*__h_generic_getticks)(void) = timer_null_getticks;
static uint64_t (*__h_generic_getres)(void)   = timer_null_getres;
static uint64_t (*__h_percpu_getticks)(void)  = timer_null_getticks;
static uint64_t (*__h_percpu_getres)(void)    = timer_null_getres;


/**
 * @brief Converts a tick count into another unit without overflowing the intermediate product.
 *
 * @param ticks Ticks counted at frequency.
 * @param scale Units per second of the result (1000000000 for ns, 1000000 for us, 1000 for ms).
 * @param frequency Ticks per second.
 * @return ticks * scale / frequency.
 */
static uint64_t timer_scale(uint64_t ticks, uint64_t scale, uint64_t frequency) {

    DEBUG_ASSERT(scale);
    DEBUG_ASSERT(frequency);
    DEBUG_ASSERT(frequency <= UINT64_MAX / scale);

    return ((ticks / frequency) * scale) + (((ticks % frequency) * scale) / frequency);
}



void arch_timer_delay(uint64_t us) {

    DEBUG_ASSERT(us > 0);
    DEBUG_ASSERT(us < 100000000); // 10sec max


    uint64_t start = arch_timer_generic_getus();

    while ((arch_timer_generic_getus() - start) < us)
        __builtin_ia32_pause();
}


uint64_t arch_timer_gettime(void) {
    return rtc_gettime();
}

uint64_t arch_timer_percpu_getticks(void) {
    return __h_percpu_getticks();
}

uint64_t arch_timer_percpu_getns(void) {
    return timer_scale(__h_percpu_getticks(), 1000000000, __h_percpu_getres());
}

uint64_t arch_timer_percpu_getus(void) {
    return timer_scale(__h_percpu_getticks(), 1000000, __h_percpu_getres());
}

uint64_t arch_timer_percpu_getms(void) {
    return timer_scale(__h_percpu_getticks(), 1000, __h_percpu_getres());
}

uint64_t arch_timer_percpu_getres(void) {
    return __h_percpu_getres();
}

uint64_t arch_timer_generic_getticks(void) {
    return __h_generic_getticks() - generic_origin;
}

uint64_t arch_timer_generic_getns(void) {
    return timer_scale(arch_timer_generic_getticks(), 1000000000, __h_generic_getres());
}

uint64_t arch_timer_generic_getus(void) {
    return timer_scale(arch_timer_generic_getticks(), 1000000, __h_generic_getres());
}

uint64_t arch_timer_generic_getms(void) {
    return timer_scale(arch_timer_generic_getticks(), 1000, __h_generic_getres());
}

uint64_t arch_timer_generic_getres(void) {
    return __h_generic_getres();
}

uint64_t arch_timer_realtime_getns(void) {

    if (unlikely(!realtime_base_ns))
        return arch_timer_gettime() * 1000000000ULL;

    return realtime_base_ns + arch_timer_generic_getns();
}




/**
 * @brief Chooses the per-CPU and generic clocks, makes sure the TSC frequency is known, and starts the generic clock at zero.
 */
void timer_init(void) {

    if (rtc_init() < 0)
        kpanicf("x86-timer: PANIC! an error occurred when initializing RTC clock, required!\n");

    if (tsc_init() < 0)
        kpanicf("x86-timer: PANIC! an error occurred when initializing TSC clock, required!\n");


    bool invariant = boot_cpu_has(X86_FEATURE_CONSTANT_TSC) && boot_cpu_has(X86_FEATURE_NONSTOP_TSC);
    bool known     = tsc_getres() != 0;

    uint64_t (*ref_getticks)(void) = NULL;
    uint64_t (*ref_getres)(void)   = NULL;

    if (!invariant || !known) {
        if (hpet_init() == 0) {
            ref_getticks = hpet_getticks;
            ref_getres   = hpet_getres;
        } else if (pmtmr_init() == 0) {
            ref_getticks = pmtmr_getticks;
            ref_getres   = pmtmr_getres;
        }
    }

    if (invariant) {
        __h_generic_getticks = tsc_getticks;
        __h_generic_getres   = tsc_getres;
    } else if (ref_getticks) {
        __h_generic_getticks = ref_getticks;
        __h_generic_getres   = ref_getres;
    } else {
        kpanicf("x86-timer: PANIC! TSC is not invariant and neither HPET nor ACPI PM timer is available, required!\n");
    }

    if (!known) {
        
        if (!ref_getticks && pit_init() == 0) {
            ref_getticks = pit_getticks;
            ref_getres   = pit_getres;
        }

        if (!ref_getticks)
            kpanicf("x86-timer: PANIC! TSC frequency is unknown and no clock is available to calibrate it, required!\n");

        if (tsc_calibrate(ref_getticks, ref_getres) < 0)
            kpanicf("x86-timer: PANIC! TSC calibration failed, required!\n");
    }


    __h_percpu_getticks = tsc_getticks;
    __h_percpu_getres   = tsc_getres;

    generic_origin   = __h_generic_getticks();
    realtime_base_ns = (arch_timer_gettime() * 1000000000ULL) - arch_timer_generic_getns();

#if DEBUG_LEVEL_INFO
    kprintf("x86-timer: now(%ld) percpu[mHZ(%lld)] generic[mHZ(%lld)]\n", arch_timer_gettime(), arch_timer_percpu_getres() / 1000000ULL, arch_timer_generic_getres() / 1000000ULL);
#endif
}



TEST(x86_timer_delay_test, {
    uint64_t now = arch_timer_generic_getus();

    arch_timer_delay(100);

    uint64_t then = arch_timer_generic_getus();

    DEBUG_ASSERT(then - now >= 100);
});

TEST(x86_timer_scale_test, {
    DEBUG_ASSERT(timer_scale(3, 10, 4) == 7);
    DEBUG_ASSERT(timer_scale(123456789012345ULL, 1000000000, 100000000) == 1234567890123450ULL);
    DEBUG_ASSERT(timer_scale(UINT64_MAX / 1000000000, 1000000000, 1000000000) == UINT64_MAX / 1000000000);
});
