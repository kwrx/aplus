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


#ifndef _APLUS_H
#define _APLUS_H

#include <aplus/base.h>

#if defined(KERNEL)

#ifndef COMMIT
#define COMMIT                              "unknown"
#endif

#define CONFIG_LFS                          0
#define CONFIG_CLOCKS_PER_SEC               1000000


#define KERNEL_NAME                         "aplus"
#define KERNEL_VERSION_MAJOR                "0"
#define KERNEL_VERSION_MINOR                "5"
#define KERNEL_DATE                         __DATE__
#define KERNEL_TIME                         __TIME__

#ifdef DEBUG
#define KERNEL_CODENAME                     COMMIT "-debug"
#else
#define KERNEL_CODENAME                     COMMIT "-generic"
#endif

    
#define KERNEL_VERSION                      \
            KERNEL_VERSION_MAJOR "."        \
            KERNEL_VERSION_MINOR "."        \
            KERNEL_CODENAME


#if DEBUG
#    undef KERNEL_CODENAME
#    define KERNEL_CODENAME                 COMMIT "-debug"
#endif


#if defined(__i386__)
#    define KERNEL_PLATFORM                 "i386"
#    define CONFIG_BITS                     32
#    define CONFIG_KERNEL_BASE              0xC0000000L
#    define CONFIG_HEAP_BASE                0xC8000000L
#    define CONFIG_STACK_BASE               0xD8000000L
#    define CONFIG_HEAP_SIZE                0x10000000L
#    define CONFIG_HEAP_PREALLOC_SIZE       0x00100000L
#    define CONFIG_STACK_SIZE               0x00010000L
#    define CONFIG_FRAME_SIZE               0x00000100L
#elif defined(__x86_64__)
#    define KERNEL_PLATFORM                 "x86-64"
#    define CONFIG_BITS                     64
#    define CONFIG_KERNEL_BASE              0xFFFFFFFF80000000L
#    define CONFIG_HEAP_BASE                0xFFFFFFFFC0000000L
#    define CONFIG_STACK_BASE               0xFFFFFFFFFFC00000L
#    define CONFIG_HEAP_SIZE                0x000000003F000000L
#    define CONFIG_STACK_SIZE               0x0000000000010000L
#elif defined(__arm__)
#    define KERNEL_PLATFORM                 "arm"
#    define CONFIG_BITS                     32
#    if defined (__rpi__)
        /* TODO */
#    else
#        define CONFIG_KERNEL_BASE          0x80010000L
#        define CONFIG_HEAP_BASE            0x8F000000L
#        define CONFIG_STACK_BASE           0x8A000000L
#        define CONFIG_HEAP_SIZE            0x00100000ULL
#        define CONFIG_STACK_SIZE           0x00004000ULL
#    endif
#endif

#ifndef KERNEL_PLATFORM
#define KERNEL_PLATFORM                     "unknown"
#endif



#define MBD_MMAP_AVAILABLE          1
#define MBD_MMAP_RESERVED           2

#define MBD_VIDEO_INDEXED           0
#define MBD_VIDEO_RGB               1
#define MBD_VIDEO_TEXT              2

#define MBD_MODULES_MAX             1024


#include <stdint.h>
#include <sys/types.h>
#include <aplus/timer.h>

typedef struct {
    struct {
        uint64_t size;
        uint64_t used;
        uint32_t pagesize;
        uintptr_t start;
    } memory;

    struct {
        struct {
            uintptr_t ptr;
            size_t size;
            uintptr_t cmdline;
            uintptr_t status;
        } __packed ptr[MBD_MODULES_MAX];
        size_t count;
    } modules;

    struct {
        struct {
            uint32_t size;
            uint64_t address;
            uint64_t length;
            uint32_t type;
        } __packed *ptr;
        size_t count;
    } mmap;

    struct {
        uint8_t type;
        uint16_t width;
        uint16_t height;
        uint16_t depth;
        uint32_t pitch;
        uintptr_t base;
        uint32_t size;
    } fb;

    struct {
        uint32_t num;
        uintptr_t addr;
        uint32_t size;
        uint32_t shndx;
    } exec;

    struct {
        uint32_t min_speed;
        uint32_t max_speed;
        uint32_t max_cores;
        uint32_t max_threads;
        const char* family;

        struct {
            uint32_t id;
            uint32_t flags;
        } cores[16];
    } cpu;

    int flags;
    int quiet;
    const char* cmdline;
} mbd_t;

extern mbd_t* mbd;

extern void core_stacktrace(void);
extern uintptr_t core_get_address(const char*);
extern const char* core_get_name(uintptr_t);


extern void arch_init(void);
extern void core_init(void);
extern void root_init(void);


void kmain(void);
void cmain(void);



#ifndef __init_ns
#define __init_ns
#endif

#define __init(x, y) {                                      \
    kprintf("[+] Initializing '" __init_ns #x "'\n");       \
    x##_init y;                                             \
}


#define __user
#define __thread_safe



#endif

#endif