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

#ifndef _APLUS_BASE_H
#define _APLUS_BASE_H



#ifdef DEBUG
    #undef DEBUG
#endif

#ifdef CONFIG_HAVE_DEBUG
    #define DEBUG             1
    #define DEBUG_LEVEL       CONFIG_DEBUG_LEVEL
    #define DEBUG_LEVEL_TRACE (DEBUG_LEVEL >= 4)
    #define DEBUG_LEVEL_INFO  (DEBUG_LEVEL >= 3)
    #define DEBUG_LEVEL_WARN  (DEBUG_LEVEL >= 2)
    #define DEBUG_LEVEL_ERROR (DEBUG_LEVEL >= 1)
    #define DEBUG_LEVEL_FATAL (DEBUG_LEVEL >= 0)
#else
    #define DEBUG_LEVEL_TRACE 0
    #define DEBUG_LEVEL_INFO  0
    #define DEBUG_LEVEL_WARN  0
    #define DEBUG_LEVEL_ERROR 0
    #define DEBUG_LEVEL_FATAL 0
#endif



#ifndef __ASSEMBLY__

    #include <stddef.h>
    #include <stdint.h>

    #ifndef likely
        #define likely(cond) __builtin_expect(!!(cond), 1)
    #endif

    #ifndef unlikely
        #define unlikely(cond) __builtin_expect(!!(cond), 0)
    #endif

    #ifndef barrier
        #define barrier() __asm__ __volatile__("" ::: "memory")
    #endif


    #ifndef __packed
        #define __packed __attribute__((packed))
    #endif

    #ifndef __pure
        #define __pure __attribute__((pure))
    #endif

    #ifndef __weak
        #define __weak __attribute__((weak))
    #endif

    #ifndef __noinline
        #define __noinline __attribute__((noinline))
    #endif

    #ifndef __always_inline
        #define __always_inline __attribute__((always_inline))
    #endif

    #ifndef __const
        #define __const __attribute__((const))
    #endif

    #ifndef __deprecated
        #define __deprecated __attribute__((deprecated))
    #endif

    #ifndef __alias
        #define __alias(s) __attribute__((alias(#s)))
    #endif

    #ifndef __aligned
        #define __aligned(x) __attribute__((aligned(x)))
    #endif

    #ifndef __malloc
        #define __malloc __attribute__((malloc))
    #endif

    #ifndef __alloc_size
        #define __alloc_size(x...) __attribute__((alloc_size(x)))
    #endif

    #ifndef __section
        #define __section(x) __attribute__((section(x)))
    #endif

    #ifndef __format
        #define __format(a, b, c) __attribute__((format(a, b, c)))
    #endif

    #ifndef __nosanitize
        #define __nosanitize(x) __attribute__((no_sanitize(x)))
    #endif

    #ifndef __no_sanitize_address
        #define __no_sanitize_address __nosanitize("address")
    #endif

    #ifndef __no_sanitize_thread
        #define __no_sanitize_thread __nosanitize("thread")
    #endif

    #ifndef __no_sanitize_undefined
        #define __no_sanitize_undefined __nosanitize("undefined")
    #endif

    #ifndef __returns_nonnull
        #define __returns_nonnull __attribute__((returns_nonnull))
    #endif

    #ifndef __noreturn
        #define __noreturn __attribute__((noreturn))
    #endif

    #ifndef __nonnull
        #define __nonnull(x...) __attribute__((nonnull(x)))
    #endif

    #ifndef __warn_unused_result
        #define __warn_unused_result __attribute__((warn_unused_result))
    #endif

    #ifndef __unused_param
        #define __unused_param(x) (void)(x)
    #endif

    #ifndef __scoped
        #define __scoped(x) __attribute__((__cleanup__(x)))
    #endif

    #define __PRAGMA(x) _Pragma(#x)
    #define WARNING(x)  __PRAGMA(GCC diagnostic ignored x)



    #ifndef __BEGIN_DECLS
        #ifdef __cplusplus
            #define __BEGIN_DECLS extern "C" {
        #else
            #define __BEGIN_DECLS
        #endif
    #endif

    #ifndef __END_DECLS
        #ifdef __cplusplus
            #define __END_DECLS }
        #else
            #define __END_DECLS
        #endif
    #endif



    #if defined(KERNEL)

        #define __kernel
        #define __safe
        #define __force
        #define __nocast
        #define __iomem
        #define __percpu

    #endif


    #if defined(KERNEL)
        #include <aplus/memory.h>
        #include <aplus/smp.h>


        #ifndef MAX
            #define MAX(a, b) ((a) > (b) ? (a) : (b))
        #endif

        #ifndef MIN
            #define MIN(a, b) ((a) < (b) ? (a) : (b))
        #endif


        #define CORE_MODULE_MAX 1024


struct syscore {

        struct {

                uintptr_t phys_upper;
                uintptr_t phys_lower;

        } memory;

        struct {

                struct {
                        uintptr_t address;
                        uintptr_t length;
                        uintptr_t type;
                } ptr[CONFIG_BUFSIZ << 2];

                size_t count;

        } mmap;

        struct {

                char cmdline[CONFIG_BUFSIZ];
                char bootloader[CONFIG_BUFSIZ];

        } boot;

        struct {

                uint16_t width;
                uint16_t height;
                uint16_t depth;
                uint32_t pitch;
                uintptr_t address;

                uint8_t red_field_position;
                uint8_t green_field_position;
                uint8_t blue_field_position;

                uint8_t red_mask_size;
                uint8_t green_mask_size;
                uint8_t blue_mask_size;

        } framebuffer;


        struct {
                uintptr_t rsdp_address;
                uintptr_t rsdp_size;
        } acpi;


        struct {

                uintptr_t load_base_address;

                uintptr_t sh_num;
                uintptr_t sh_entsize;
                uintptr_t sh_shndx;

                char sections[CONFIG_BUFSIZ << 2];

        } exe;


        struct {

                struct {

                        uintptr_t ptr;
                        uintptr_t size;
                        uintptr_t status;

                        char cmdline[CONFIG_BUFSIZ];

                } __packed ko[CORE_MODULE_MAX];

                size_t count;

        } modules;


        struct {

                uint16_t max_cores;
                uint16_t max_threads;
                uint16_t max_mhz;
                uint16_t min_mhz;

                cpu_t cores[SMP_CPU_MAX];

        } cpu;

        #define bsp cpu.cores[SMP_CPU_BOOTSTRAP_ID]
};


__BEGIN_DECLS


//? See kernel/kmain.c
extern struct syscore* core;

//? See kernel/runtime/dl.c
uintptr_t runtime_get_address(const char*) __nonnull((1));
const char* runtime_get_name(uintptr_t);

//? See kernel/runtime/stacktrace.c
void runtime_stacktrace(void);

//? See kernel/runtime/dump.c
void runtime_dump(void);


__END_DECLS

    #endif

#endif
#endif
