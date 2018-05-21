#ifndef _CONFIG_H
#define _CONFIG_H

#include <aplus/base.h>

/* Makefile generated */
#define DEBUG 1
#define COMMIT "e2b6cda1"
#define PLATFORM "i386"
#define TARGET "i686-aplus"
/**********************/

#define CONFIG_HOST_MEMORY                  512
#define CONFIG_HAVE_LIBC                    1
#define CONFIG_SMP                          0    /* TODO */
#define CONFIG_IPC                          1
#define CONFIG_IPC_TIMEOUT                  60000
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



#ifdef __GNUC__
#    ifndef __weak
#        define __weak                      __attribute__((weak))
#    endif

#    ifndef __packed
#        define __packed                    __attribute__((packed))
#    endif

#    ifndef __section
#        define __section(x)                __attribute__((section(x)))
#    endif

#    ifndef __align
#        define __align(x)                  __attribute__((align(x)))
#    endif

#    ifndef __malloc
#        define __malloc                    __attribute__((malloc))
#    endif

#    ifndef __optimize
#        define __optimize(x)               __attribute__((optimize("O" #x)))
#    endif

#    define __PRAGMA(x)                     _Pragma(#x)
#    define WARNING(x)                      __PRAGMA(GCC diagnostic ignored x)
#else
#    define __weak
#    define __packed
#    define __section(x)
#    define __align(x)
#    define __malloc
#    define __optimize(x)
#    define WARNING(x)
#endif


#endif


