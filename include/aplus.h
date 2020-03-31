#ifndef _APLUS_BASE_H
#define _APLUS_BASE_H



#ifdef DEBUG
#undef DEBUG
#endif

#ifdef CONFIG_HAVE_DEBUG
#define DEBUG           1
#define DEBUG_LEVEL     CONFIG_DEBUG_LEVEL
#endif


#ifndef __ASSEMBLY__

#include <stdint.h>
#include <stddef.h>

#ifndef likely
#define likely(cond)    __builtin_expect(!!(cond), 1)
#endif

#ifndef unlikely
#define unlikely(cond)  __builtin_expect(!!(cond), 0)
#endif

#ifndef barrier
#define barrier()       __asm__ __volatile__("" ::: "memory")
#endif



#ifndef __packed
#define __packed        __attribute__((packed))
#endif

#ifndef __pure
#define __pure          __attribute__((pure))
#endif

#ifndef __weak
#define __weak          __attribute__((weak))
#endif

#ifndef __deprecated
#define __deprecated    __attribute__((deprecated))
#endif

#ifndef __noreturn
#define __noreturn      __attribute__((noreturn))
#endif

#ifndef __alias
#define __alias(s)      __attribute__((alias(#s)))
#endif


#define __PRAGMA(x)     _Pragma(#x)
#define WARNING(x)      __PRAGMA(GCC diagnostic ignored x)



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
#if defined(DEBUG) && DEBUG_LEVEL >= 1

#if 0 //! FIXME !!
#define __user          __attribute__((noderef, address_space(1)))
#define __kernel        __attribute__((address_space(0)))
#define __safe          __attribute__((safe))
#define __force         __attribute__((force))
#define __nocast        __attribute__((nocast))
#define __iomem         __attribute__((noderef, address_space(2)))
#define __percpu        __attribute__((noderef, address_space(3)))
#endif 

#define __user
#define __kernel
#define __safe
#define __force
#define __nocast
#define __iomem
#define __percpu

#define __must_hold(x)  __attribute__((context(x,1,1)))
#define __acquires(x)   __attribute__((context(x,0,1)))
#define __releases(x)   __attribute__((context(x,1,0)))
#define __acquire(x)    __context__(x,1)
#define __release(x)    __context__(x,-1)

#define __cond_lock(x, c)    \
    ((c) ? ({ __acquire(x); 1; }) : 0)


extern void __chk_user_ptr(const volatile void __user *);
extern void __chk_io_ptr(const volatile void __iomem *);

#else

#define __user
#define __kernel
#define __safe
#define __force
#define __nocast
#define __iomem
#define __chk_user_ptr(x)               (void) 0
#define __chk_io_ptr(x)                 (void) 0
#define __builtin_warning(x, y...)      (1)
#define __must_hold(x)
#define __acquires(x)
#define __releases(x)
#define __acquire(x)                    (void) 0
#define __release(x)                    (void) 0
#define __cond_lock(x, c)               (c)
#define __percpu

#endif


#if defined(KERNEL)
#include <aplus/memory.h>
#include <aplus/smp.h>


#define CORE_MODULE_MAX                 1024


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
uintptr_t   runtime_get_address(const char*);
const char* runtime_get_name(uintptr_t);

//? See kernel/runtime/stacktrace.c
void runtime_stacktrace(void);


__END_DECLS

#endif

#endif
#endif
#endif