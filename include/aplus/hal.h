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


#ifndef _APLUS_HAL_H
#define _APLUS_HAL_H


#if defined(KERNEL)
#include <aplus.h>
#include <aplus/task.h>


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




#ifndef __ASSEMBLY__

#define uio_check(p, m)    \
    (arch_vmm_access(current_task->address_space, (uintptr_t) (p), (int) (m)) == 0 ? 1 : 0)


#define uio_get_ptr(p)              \
    ((uintptr_t) arch_vmm_p2v(arch_vmm_v2p((uintptr_t) (p), ARCH_VMM_AREA_USER), ARCH_VMM_AREA_HEAP))

#define uio_r8(p)                  (*(uint8_t  volatile*) (uio_get_ptr(p)))
#define uio_r16(p)                 (*(uint16_t volatile*) (uio_get_ptr(p)))
#define uio_r32(p)                 (*(uint32_t volatile*) (uio_get_ptr(p)))
#define uio_r64(p)                 (*(uint64_t volatile*) (uio_get_ptr(p)))

#define uio_w8(p, v)               { uio_r8(p)  = (uint8_t)  (v); }
#define uio_w16(p, v)              { uio_r16(p) = (uint16_t) (v); }
#define uio_w32(p, v)              { uio_r32(p) = (uint32_t) (v); }
#define uio_w64(p, v)              { uio_r64(p) = (uint64_t) (v); }



__BEGIN_DECLS

void arch_cpu_init(int);
void arch_cpu_startup(int);
uint64_t arch_cpu_get_current_id(void);


void arch_debug_init(void);
void arch_debug_putc(char);
void arch_debug_stacktrace(uintptr_t*, size_t);


void arch_intr_enable(long);
long arch_intr_disable(void);
void arch_intr_map_irq(uint8_t, void (*) (void*, uint8_t));
void arch_intr_unmap_irq(uint8_t);


void arch_task_switch(task_t*, task_t*);
pid_t arch_task_spawn_init(void);
pid_t arch_task_spawn_kthread(const char*, void (*) (void*), size_t, void*);
task_t* arch_task_get_empty_thread(size_t);
void arch_task_context_set(task_t*, int, long);
long arch_task_context_get(task_t*, int);


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
uintptr_t arch_vmm_gethugepagesize();
uintptr_t arch_vmm_p2v(uintptr_t, int);
uintptr_t arch_vmm_v2p(uintptr_t, int);
uintptr_t arch_vmm_map(vmm_address_space_t*, uintptr_t, uintptr_t, size_t, int);
uintptr_t arch_vmm_unmap(vmm_address_space_t*, uintptr_t, size_t);
uintptr_t arch_vmm_mprotect(vmm_address_space_t*, uintptr_t, size_t, int);
int arch_vmm_access(vmm_address_space_t*, uintptr_t, int);
uintptr_t arch_vmm_getphysaddr(vmm_address_space_t*, uintptr_t);
void arch_vmm_clone(vmm_address_space_t*, vmm_address_space_t*, int);


__END_DECLS

#endif


#endif
#endif
