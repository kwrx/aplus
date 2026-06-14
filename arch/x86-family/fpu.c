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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/fpu.h>
#include <arch/x86/intr.h>


#define FPU_INITIAL_STATE_SIZE 8192
#define FPU_FXSAVE_SIZE        512
#define FPU_FSAVE_SIZE         108

#define FPU_XSAVE_ALIGNMENT  64
#define FPU_LEGACY_ALIGNMENT 16

#define XCR0_FPU    (1ULL << 0)
#define XCR0_SSE    (1ULL << 1)
#define XCR0_AVX    (1ULL << 2)
#define XCR0_OPMASK (1ULL << 5)
#define XCR0_ZMM    (1ULL << 6)
#define XCR0_ZMM2   (1ULL << 7)


enum fpu_mode {
    FPU_MODE_NONE,
    FPU_MODE_FSAVE,
    FPU_MODE_FXSAVE,
    FPU_MODE_XSAVE,
    FPU_MODE_XSAVEOPT,
};


static void (*__fpu_switch)(void*, void*) = NULL;
static void (*__fpu_save)(void*)          = NULL;
static void (*__fpu_restore)(void*)       = NULL;

static uint8_t __fpu_initial_state[FPU_INITIAL_STATE_SIZE] __aligned(FPU_XSAVE_ALIGNMENT) = {0};
static enum fpu_mode __fpu_mode                                                           = FPU_MODE_NONE;
static size_t __fpu_alignment                                                             = 0;
static size_t __fpu_size                                                                  = 0;
static uint64_t __fpu_xcr0                                                                = 0;


static void fpu_validate_area(const void* fpu_area) {

    if (unlikely(!fpu_area || !__fpu_alignment || ((uintptr_t)fpu_area & (__fpu_alignment - 1))))
        kpanicf("x86-fpu: invalid FPU state area %p, required alignment %zd\n", fpu_area, __fpu_alignment);
}


static void* fpu_new_buffer(size_t prefix) {

    if (unlikely(!__fpu_size || !__fpu_alignment || (prefix & (sizeof(void*) - 1))))
        kpanicf("x86-fpu: invalid state buffer configuration\n");

    if (unlikely(prefix > SIZE_MAX - __fpu_size || prefix + __fpu_size > SIZE_MAX - (__fpu_alignment - 1) - sizeof(void*)))
        kpanicf("x86-fpu: FPU state allocation size overflow\n");

    size_t allocation_size = prefix + __fpu_size + (__fpu_alignment - 1) + sizeof(void*);
    void* allocation       = kcalloc(1, allocation_size, GFP_KERNEL);

    if (unlikely(!allocation))
        kpanicf("x86-fpu: failed to allocate %zd-byte FPU state buffer\n", allocation_size);

    uintptr_t state = ((uintptr_t)allocation + sizeof(void*) + prefix + (__fpu_alignment - 1)) & ~((uintptr_t)__fpu_alignment - 1);
    void* buffer    = (void*)(state - prefix);

    ((void**)buffer)[-1] = allocation;

    return buffer;
}


static void fpu_free_buffer(void* buffer) {

    if (unlikely(!buffer))
        kpanicf("x86-fpu: cannot free null FPU state buffer\n");

    kfree(((void**)buffer)[-1]);
}


#if !defined(CONFIG_X86_XSAVE_FORCE_DISABLED)

static uint64_t fpu_xsave_supported_mask(void) {

    long a, b, c, d;
    x86_cpuid(0xD, &a, &b, &c, &d);

    return ((uint64_t)(uint32_t)d << 32) | (uint32_t)a;
}


static size_t fpu_xsave_enabled_size(void) {

    long a, b, c, d;
    x86_cpuid(0xD, &a, &b, &c, &d);

    return (size_t)(uint32_t)b;
}

#endif


static void fpu_configure_control_registers(uint64_t cpu, int xsave) {

    if (unlikely(!cpu_has(cpu, X86_FEATURE_FPU)))
        kpanicf("x86-fpu: CPU %ld does not support an FPU\n", cpu);

    x86_set_cr0((x86_get_cr0() & ~(X86_CR0_EM_MASK | X86_CR0_TS_MASK)) | X86_CR0_MP_MASK);

    if (__fpu_mode == FPU_MODE_FXSAVE || __fpu_mode == FPU_MODE_XSAVE || __fpu_mode == FPU_MODE_XSAVEOPT) {

        if (unlikely(!cpu_has(cpu, X86_FEATURE_FXSR)))
            kpanicf("x86-fpu: CPU %ld does not support the selected FXSAVE state format\n", cpu);

        x86_set_cr4(x86_get_cr4() | X86_CR4_OSFXSR_MASK);

        if (cpu_has(cpu, X86_FEATURE_XMM))
            x86_set_cr4(x86_get_cr4() | X86_CR4_OSXMMEXCPT_MASK);
    }

    if (xsave) {

        if (unlikely(!cpu_has(cpu, X86_FEATURE_XSAVE)))
            kpanicf("x86-fpu: CPU %ld does not support the selected XSAVE state format\n", cpu);

        x86_set_cr4(x86_get_cr4() | X86_CR4_OSXSAVE_MASK);
    }
}


#if !defined(CONFIG_X86_XSAVE_FORCE_DISABLED)

static void xsave_write(void* fpu_area, int optimized) {

    fpu_validate_area(fpu_area);

    uint32_t low  = (uint32_t)__fpu_xcr0;
    uint32_t high = (uint32_t)(__fpu_xcr0 >> 32);

    if (optimized)
        __asm__ __volatile__("xsaveopt (%0)" : : "r"(fpu_area), "a"(low), "d"(high) : "memory");
    else
        __asm__ __volatile__("xsave (%0)" : : "r"(fpu_area), "a"(low), "d"(high) : "memory");
}


static void xsave_read(void* fpu_area) {

    fpu_validate_area(fpu_area);

    uint32_t low  = (uint32_t)__fpu_xcr0;
    uint32_t high = (uint32_t)(__fpu_xcr0 >> 32);

    __asm__ __volatile__("xrstor (%0)" : : "r"(fpu_area), "a"(low), "d"(high) : "memory");
}


static void xsaveopt_switch(void* prev, void* next) {

    xsave_write(prev, 1);
    xsave_read(next);
}


static void xsave_switch(void* prev, void* next) {

    xsave_write(prev, 0);
    xsave_read(next);
}


static void xsave_save(void* fpu_area) {
    xsave_write(fpu_area, 0);
}


static void xsave_restore(void* fpu_area) {
    xsave_read(fpu_area);
}

#endif


#if !defined(CONFIG_X86_FXSAVE_FORCE_DISABLED)

static void fxsave_switch(void* prev, void* next) {

    fpu_validate_area(prev);
    fpu_validate_area(next);

    __asm__ __volatile__("fxsave (%0)" : : "r"(prev) : "memory");
    __asm__ __volatile__("fxrstor (%0)" : : "r"(next) : "memory");
}


static void fxsave_save(void* fpu_area) {

    fpu_validate_area(fpu_area);
    __asm__ __volatile__("fxsave (%0)" : : "r"(fpu_area) : "memory");
}


static void fxsave_restore(void* fpu_area) {

    fpu_validate_area(fpu_area);
    __asm__ __volatile__("fxrstor (%0)" : : "r"(fpu_area) : "memory");
}

#endif


static void fsave_switch(void* prev, void* next) {

    fpu_validate_area(prev);
    fpu_validate_area(next);

    __asm__ __volatile__("fsave (%0)" : : "r"(prev) : "memory");
    __asm__ __volatile__("frstor (%0)" : : "r"(next) : "memory");
}


static void fsave_save(void* fpu_area) {

    fpu_validate_area(fpu_area);
    __asm__ __volatile__("fsave (%0)" : : "r"(fpu_area) : "memory");
}


static void fsave_restore(void* fpu_area) {

    fpu_validate_area(fpu_area);
    __asm__ __volatile__("frstor (%0)" : : "r"(fpu_area) : "memory");
}


static void fpu_select_bsp_mode(void) {

#if !defined(CONFIG_X86_XSAVE_FORCE_DISABLED)

    if (boot_cpu_has(X86_FEATURE_XSAVE) && boot_cpu_has(X86_FEATURE_FXSR)) {

        uint64_t supported = fpu_xsave_supported_mask();

        if (unlikely(!(supported & XCR0_FPU)))
            kpanicf("x86-fpu: XSAVE does not support mandatory x87 state\n");

        __fpu_xcr0 = XCR0_FPU;

        if (boot_cpu_has(X86_FEATURE_XMM) && ((supported & XCR0_SSE) == XCR0_SSE))
            __fpu_xcr0 |= XCR0_SSE;

        if (boot_cpu_has(X86_FEATURE_AVX) && ((supported & (XCR0_SSE | XCR0_AVX)) == (XCR0_SSE | XCR0_AVX)))
            __fpu_xcr0 |= XCR0_SSE | XCR0_AVX;

        if (boot_cpu_has(X86_FEATURE_AVX512F) && ((supported & (XCR0_SSE | XCR0_AVX | XCR0_OPMASK | XCR0_ZMM | XCR0_ZMM2)) == (XCR0_SSE | XCR0_AVX | XCR0_OPMASK | XCR0_ZMM | XCR0_ZMM2)))
            __fpu_xcr0 |= XCR0_SSE | XCR0_AVX | XCR0_OPMASK | XCR0_ZMM | XCR0_ZMM2;

        __fpu_mode      = boot_cpu_has(X86_FEATURE_XSAVEOPT) ? FPU_MODE_XSAVEOPT : FPU_MODE_XSAVE;
        __fpu_alignment = FPU_XSAVE_ALIGNMENT;

        fpu_configure_control_registers(SMP_CPU_BOOTSTRAP_ID, 1);
        x86_xsetbv(0, __fpu_xcr0);

        __fpu_size = fpu_xsave_enabled_size();

        if (unlikely(!__fpu_size || __fpu_size > sizeof(__fpu_initial_state)))
            kpanicf("x86-fpu: invalid XSAVE area size %zd, maximum supported is %zd\n", __fpu_size, sizeof(__fpu_initial_state));

        __fpu_switch  = __fpu_mode == FPU_MODE_XSAVEOPT ? &xsaveopt_switch : &xsave_switch;
        __fpu_save    = &xsave_save;
        __fpu_restore = &xsave_restore;

        return;
    }

#endif


#if !defined(CONFIG_X86_FXSAVE_FORCE_DISABLED)

    if (boot_cpu_has(X86_FEATURE_FXSR)) {

        __fpu_mode      = FPU_MODE_FXSAVE;
        __fpu_alignment = FPU_LEGACY_ALIGNMENT;
        __fpu_size      = FPU_FXSAVE_SIZE;
        __fpu_switch    = &fxsave_switch;
        __fpu_save      = &fxsave_save;
        __fpu_restore   = &fxsave_restore;

        fpu_configure_control_registers(SMP_CPU_BOOTSTRAP_ID, 0);

        return;
    }

#endif


    __fpu_mode      = FPU_MODE_FSAVE;
    __fpu_alignment = FPU_LEGACY_ALIGNMENT;
    __fpu_size      = FPU_FSAVE_SIZE;
    __fpu_switch    = &fsave_switch;
    __fpu_save      = &fsave_save;
    __fpu_restore   = &fsave_restore;

    fpu_configure_control_registers(SMP_CPU_BOOTSTRAP_ID, 0);
}


static void fpu_configure_ap(uint64_t cpu) {

    int xsave = __fpu_mode == FPU_MODE_XSAVE || __fpu_mode == FPU_MODE_XSAVEOPT;

    fpu_configure_control_registers(cpu, xsave);

#if !defined(CONFIG_X86_XSAVE_FORCE_DISABLED)

    if (xsave) {

        if (unlikely(__fpu_mode == FPU_MODE_XSAVEOPT && !cpu_has(cpu, X86_FEATURE_XSAVEOPT)))
            kpanicf("x86-fpu: CPU %ld does not support selected XSAVEOPT mode\n", cpu);

        if (unlikely((__fpu_xcr0 & XCR0_SSE) && !cpu_has(cpu, X86_FEATURE_XMM)))
            kpanicf("x86-fpu: CPU %ld does not support selected SSE state\n", cpu);

        if (unlikely((__fpu_xcr0 & XCR0_AVX) && !cpu_has(cpu, X86_FEATURE_AVX)))
            kpanicf("x86-fpu: CPU %ld does not support selected AVX state\n", cpu);

        if (unlikely((__fpu_xcr0 & (XCR0_OPMASK | XCR0_ZMM | XCR0_ZMM2)) && !cpu_has(cpu, X86_FEATURE_AVX512F)))
            kpanicf("x86-fpu: CPU %ld does not support selected AVX-512 state\n", cpu);

        uint64_t supported = fpu_xsave_supported_mask();

        if (unlikely((supported & __fpu_xcr0) != __fpu_xcr0))
            kpanicf("x86-fpu: CPU %ld does not support selected XCR0 mask 0x%lX\n", cpu, __fpu_xcr0);

        x86_xsetbv(0, __fpu_xcr0);

        size_t size = fpu_xsave_enabled_size();

        if (unlikely(size != __fpu_size))
            kpanicf("x86-fpu: CPU %ld XSAVE area size %zd differs from BSP size %zd\n", cpu, size, __fpu_size);
    }

#else

    (void)xsave;

#endif
}


void fpu_init(uint64_t cpu) {

    if (cpu == SMP_CPU_BOOTSTRAP_ID)
        fpu_select_bsp_mode();
    else {

        if (unlikely(__fpu_mode == FPU_MODE_NONE))
            kpanicf("x86-fpu: AP initialized before BSP FPU mode selection\n");

        fpu_configure_ap(cpu);
    }

    __asm__ __volatile__("fninit" : : : "memory");

    if (cpu == SMP_CPU_BOOTSTRAP_ID) {

        fpu_save(&__fpu_initial_state[0]);

#if DEBUG_LEVEL_INFO
        const char* mode = __fpu_mode == FPU_MODE_XSAVEOPT ? "XSAVEOPT/XRSTOR" : __fpu_mode == FPU_MODE_XSAVE ? "XSAVE/XRSTOR" : __fpu_mode == FPU_MODE_FXSAVE ? "FXSAVE/FXRSTOR" : "FSAVE/FRSTOR";
        kprintf("x86-fpu: uses %s with %zd bytes, alignment %zd, XCR0 0x%lX\n", mode, __fpu_size, __fpu_alignment, __fpu_xcr0);
#endif
    }
}


void fpu_switch(void* prev, void* next) {

    if (unlikely(!__fpu_switch))
        kpanicf("x86-fpu: switch requested before initialization\n");

    __fpu_switch(prev, next);
}


void fpu_save(void* fpu_area) {

    if (unlikely(!__fpu_save))
        kpanicf("x86-fpu: save requested before initialization\n");

    __fpu_save(fpu_area);
}


void fpu_restore(void* fpu_area) {

    if (unlikely(!__fpu_restore))
        kpanicf("x86-fpu: restore requested before initialization\n");

    __fpu_restore(fpu_area);
}


size_t fpu_size(void) {

    if (unlikely(!__fpu_size))
        kpanicf("x86-fpu: size requested before initialization\n");

    return __fpu_size;
}


void* fpu_new_state(void) {

    void* state = fpu_new_buffer(0);
    memcpy(state, &__fpu_initial_state, fpu_size());

    return state;
}


void fpu_free_state(void* fpu_area) {
    fpu_free_buffer(fpu_area);
}


void* fpu_new_signal_state(void) {
    return fpu_new_buffer(sizeof(sigcontext_frame_t));
}


void fpu_free_signal_state(void* signal_state) {
    fpu_free_buffer(signal_state);
}
