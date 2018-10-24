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


#ifndef _CONFIG_H
#define _CONFIG_H

#include <aplus/base.h>

/* Makefile generated */
#define DEBUG 1
#define COMMIT "091ef68c"
#define PLATFORM "i386"
#define TARGET "i686-aplus"
/**********************/

#define CONFIG_HOST_MEMORY                  512
#define CONFIG_HAVE_LIBC                    1
#define CONFIG_SMP                          0    /* TODO */
#define CONFIG_IPC                          1
#define CONFIG_IPC_TIMEOUT                  60000
#define CONFIG_IPC_PIPEMAX                  4194304
#define CONFIG_VMM                          1
#define CONFIG_CACHE                        0    /* FIXME */
#define CONFIG_NETWORK                      1
#define CONFIG_IOSCHED                      0    /* FIXME */
#define CONFIG_CLOCKS_PER_SEC               1000000


#define CONFIG_BOCHS                        DEBUG
#define CONFIG_SERIAL_DEBUG                 DEBUG
#define CONFIG_IPC_DEBUG                    0
#define CONFIG_SYSCALL_DEBUG                0




#define KERNEL_NAME                         "aplus"
#define KERNEL_VERSION_MAJOR                "0"
#define KERNEL_VERSION_MINOR                "4"
#define KERNEL_CODENAME                     COMMIT "-generic"
#define KERNEL_DATE                         __DATE__
#define KERNEL_TIME                         __TIME__
#define KERNEL_PLATFORM                     PLATFORM

    
#define KERNEL_VERSION                      \
            KERNEL_VERSION_MAJOR "."        \
            KERNEL_VERSION_MINOR "."        \
            KERNEL_CODENAME


#if DEBUG
#    undef KERNEL_CODENAME
#    define KERNEL_CODENAME                 COMMIT "-debug"
#endif


#if defined(__i386__)
#    define CONFIG_BITS                     32
#    define CONFIG_KERNEL_BASE              0xC0000000L
#    define CONFIG_HEAP_BASE                0xC4000000L
#    define CONFIG_STACK_BASE               0xFFC00000L
#    define CONFIG_HEAP_SIZE                ((CONFIG_HOST_MEMORY * 1024 * 1024) / 2)
#    define CONFIG_STACK_SIZE               0x00020000ULL
#    define __pause__()                     __asm__ __volatile__ ("pause; hlt" ::: "memory")
#elif defined(__x86_64__)
#    define CONFIG_BITS                     64
#    define CONFIG_KERNEL_BASE              0xFFFFFFFFF8000000L
#    define CONFIG_HEAP_BASE                0xFFFFFFFFF9000000L
#    define CONFIG_STACK_BASE               0xFFFFFFFFFFC00000L
#    define CONFIG_HEAP_SIZE                ((CONFIG_HOST_MEMORY * 1024 * 1024) / 2)
#    define CONFIG_STACK_SIZE               0x00020000ULL
#    define __pause__()                     __asm__ __volatile__ ("pause; hlt" ::: "memory")
#elif defined(__arm__)
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
#    define __pause__()                     __asm__ __volatile__ ("wfe")
#endif




#endif


