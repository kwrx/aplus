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


#ifndef _APLUS_HAL_H
#define _APLUS_HAL_H


#if defined(KERNEL)
#include <stdint.h>
#include <signal.h>
#include <aplus.h>
#include <aplus/task.h>



#if defined(__i386__) || defined(__x86_64__)
#   define __atomic_membarrier()    __asm__ __volatile__("mfence" : : : "memory")
#   define __cpu_pause()            __asm__ __volatile__("pause"  : : : "memory")
#   define __cpu_halt()             __asm__ __volatile__("hlt"    : : : "memory")
#elif defined(__arm__) || defined(__aarch64__)
#   define __atomic_membarrier()    __builtin_arm_dmb(15)
#   define __cpu_pause()            __builtin_arm_wfe()
#   define __cpu_halt()             __builtin_arm_wfi()
#else
#   warning "Unknown architecture"
#   define __atomic_membarrier()    __asm__ __volatile__ ("" : : : "memory")
#   define __cpu_pause()            __asm__ __volatile__ ("" : : : "memory")
#   define __cpu_halt()             __asm__ __volatile__ ("" : : : "memory")
#endif



#define ARCH_REBOOT_RESTART         0
#define ARCH_REBOOT_SUSPEND         1
#define ARCH_REBOOT_POWEROFF        2
#define ARCH_REBOOT_HALT            3


#define ARCH_VMM_AREA_HEAP          1
#define ARCH_VMM_AREA_KERNEL        2
#define ARCH_VMM_AREA_USER          3


#define ARCH_TASK_CONTEXT_COPY      0
#define ARCH_TASK_CONTEXT_PC        1
#define ARCH_TASK_CONTEXT_STACK     2
#define ARCH_TASK_CONTEXT_RETVAL    3
#define ARCH_TASK_CONTEXT_PARAM0    4
#define ARCH_TASK_CONTEXT_PARAM1    5
#define ARCH_TASK_CONTEXT_PARAM2    6
#define ARCH_TASK_CONTEXT_PARAM3    7
#define ARCH_TASK_CONTEXT_PARAM4    8
#define ARCH_TASK_CONTEXT_PARAM5    9


#ifndef R_OK
#define R_OK                        4
#endif

#ifndef W_OK
#define W_OK                        2
#endif

#ifndef X_OK
#define X_OK                        1
#endif

#ifndef S_OK
#define S_OK                        64
#endif




#ifndef __ASSEMBLY__

#define uio_check(p, m)                 \
    (arch_vmm_access(current_task->address_space, (uintptr_t) (p), (int) (m)) == 0 ? 1 : 0)

#define uio_get_ptr(p)                  \
    ((typeof((p))) arch_vmm_p2v(arch_vmm_v2p((uintptr_t) (p), ARCH_VMM_AREA_USER), ARCH_VMM_AREA_HEAP))



#define uio_r8(p)                   (*(uint8_t  volatile*)  (uio_get_ptr(p)))
#define uio_r16(p)                  (*(uint16_t volatile*)  (uio_get_ptr(p)))
#define uio_r32(p)                  (*(uint32_t volatile*)  (uio_get_ptr(p)))
#define uio_r64(p)                  (*(uint64_t volatile*)  (uio_get_ptr(p)))
#define uio_rptr(p)                 (*(uintptr_t volatile*) (uio_get_ptr(p)))

#define uio_w8(p, v)                { uio_r8(p)   = (uint8_t)  (v);  }
#define uio_w16(p, v)               { uio_r16(p)  = (uint16_t) (v);  }
#define uio_w32(p, v)               { uio_r32(p)  = (uint32_t) (v);  }
#define uio_w64(p, v)               { uio_r64(p)  = (uint64_t) (v);  }
#define uio_wptr(p, v)              { uio_rptr(p) = (uintptr_t) (v); }

#define uio_lock(ptr, size)         { arch_vmm_lock(current_task->address_space, (uintptr_t) (ptr), (size_t) (size)); }
#define uio_unlock(ptr, size)       { arch_vmm_unlock(current_task->address_space, (uintptr_t) (ptr), (size_t) (size)); }

__BEGIN_DECLS


void arch_cpu_init(int);
void arch_cpu_startup(int);
uint64_t arch_cpu_get_current_id(void);


void arch_debug_init(void);
void arch_debug_putc(char);
void arch_debug_stacktrace(uintptr_t*, size_t);


void arch_intr_enable(long);
long arch_intr_disable(void);
void arch_intr_map_irq(irq_t, void (*) (void*, irq_t));
void arch_intr_unmap_irq(irq_t);
void arch_intr_map_irq_without_ioapic(irq_t, void (*) (void*, irq_t));
void arch_intr_unmap_irq_without_ioapic(irq_t);


void arch_task_switch(task_t*, task_t*);
pid_t arch_task_spawn_init(void);
pid_t arch_task_spawn_kthread(const char*, void (*) (void*), size_t, void*);
task_t* arch_task_get_empty_thread(size_t);
void arch_task_prepare_to_signal(siginfo_t* siginfo);
long arch_task_return_from_signal(void);
void arch_task_context_set(task_t*, int, long);
long arch_task_context_get(task_t*, int);
void arch_task_switch_address_space(vmm_address_space_t*);


void arch_reboot(int);


void arch_timer_delay(uint64_t);
uint64_t arch_timer_gettime(void);

uint64_t arch_timer_percpu_getticks(void);
uint64_t arch_timer_percpu_getns(void);
uint64_t arch_timer_percpu_getus(void);
uint64_t arch_timer_percpu_getms(void);
uint64_t arch_timer_percpu_getres(void);

uint64_t arch_timer_generic_getticks(void);
uint64_t arch_timer_generic_getns(void);
uint64_t arch_timer_generic_getus(void);
uint64_t arch_timer_generic_getms(void);
uint64_t arch_timer_generic_getres(void);


void arch_userspace_enter(uintptr_t, uintptr_t, void*);

uintptr_t arch_vmm_getpagesize();
uintptr_t arch_vmm_gethugepagesize(uint64_t);
uintptr_t arch_vmm_p2v(uintptr_t, int);
uintptr_t arch_vmm_v2p(uintptr_t, int);
uintptr_t arch_vmm_map(vmm_address_space_t*, uintptr_t, uintptr_t, size_t, int);
uintptr_t arch_vmm_unmap(vmm_address_space_t*, uintptr_t, size_t);
uintptr_t arch_vmm_mprotect(vmm_address_space_t*, uintptr_t, size_t, int);
int arch_vmm_access(vmm_address_space_t*, uintptr_t, int);
uintptr_t arch_vmm_getphysaddr(vmm_address_space_t*, uintptr_t);
void arch_vmm_lock(vmm_address_space_t*, uintptr_t, size_t);
void arch_vmm_unlock(vmm_address_space_t*, uintptr_t, size_t);
void arch_vmm_clone(vmm_address_space_t*, vmm_address_space_t*, int);






static inline void uio_memcpy_u2s(void* dst, const void* __user src, size_t n) {
    
    size_t i = 0;

    uintptr_t d = (uintptr_t) dst;
    uintptr_t s = (uintptr_t) src;


    // for(; i + 8 < n; i += 8) {
    //     *(uint64_t*) (d + i) = uio_r64(s + i);
    // }

    // for(; i + 4 < n; i += 4) {
    //     *(uint32_t*) (d + i) = uio_r32(s + i);
    // }

    // for(; i + 2 < n; i += 2) {
    //     *(uint16_t*) (d + i) = uio_r16(s + i);
    // }

    for(; i < n; i++) {
        *(uint8_t*)  (d + i) = uio_r8(s + i);
    }

}

static inline void uio_memcpy_s2u(void* __user dst, const void* src, size_t n) {
    
    size_t i = 0;

    uintptr_t d = (uintptr_t) dst;
    uintptr_t s = (uintptr_t) src;


    // for(; i + 8 < n; i += 8) {
    //     uio_w64(d + i, *(uint64_t*) (s + i));
    // }

    // for(; i + 4 < n; i += 4) {
    //     uio_w32(d + i, *(uint32_t*) (s + i));
    // }

    // for(; i + 2 < n; i += 2) {
    //     uio_w16(d + i, *(uint16_t*) (s + i));
    // }

    for(; i < n; i++) {
        uio_w8(d + i, *(uint8_t*) (s + i));
    }

}

static inline void uio_memcpy_u2u(void* __user dst, const void* __user src, size_t n) {
    
    size_t i = 0;

    uintptr_t d = (uintptr_t) dst;
    uintptr_t s = (uintptr_t) src;


    // for(; i + 8 < n; i += 8) {
    //     uio_w64(d + i, uio_r64(s + i));
    // }

    // for(; i + 4 < n; i += 4) {
    //     uio_w32(d + i, uio_r32(s + i));
    // }

    // for(; i + 2 < n; i += 2) {
    //     uio_w16(d + i, uio_r16(s + i));
    // }

    for(; i < n; i++) {
        uio_w8(d + i, uio_r8(s + i));
    }

}


static inline void uio_strcpy_u2s(char* dst, const char* __user src) {

    for(; uio_r8(src); src++, dst++) {
        *dst = uio_r8(src);
    }

    *dst = '\0';

}

static inline void uio_strcpy_s2u(char* __user dst, const char* src) {

    for(; *src; src++, dst++) {
        uio_w8(dst, *src);
    }

    uio_w8(dst, '\0');

}

static inline void uio_strcpy_u2u(char* __user dst, const char* __user src) {

    for(; uio_r8(src); src++) {
        uio_w8(dst, uio_r8(src));
    }

    uio_w8(dst, '\0');

}

static inline void uio_strncpy_u2s(char* dst, const char* __user src, size_t size) {

    for(; uio_r8(src) && --size; src++, dst++) {
        *dst = uio_r8(src);
    }

    *dst = '\0';

}

static inline void uio_strncpy_s2u(char* __user dst, const char* src, size_t size) {

    for(; *src && --size; src++, dst++) {
        uio_w8(dst, *src);
    }

    uio_w8(dst, '\0');

}

static inline void uio_strncpy_u2u(char* __user dst, const char* __user src, size_t size) {

    for(; uio_r8(src) && --size; src++, dst++) {
        uio_w8(dst, uio_r8(src));
    }

    uio_w8(dst, '\0');

}

static inline size_t uio_strlen(const char* __user s) {

    size_t k = 0;

    for(; uio_r8(s); s++) {
        k++;
    }

    return k;

}



__END_DECLS

#endif


#endif
#endif
