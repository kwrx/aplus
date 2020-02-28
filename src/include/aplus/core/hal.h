#ifndef _APLUS_CORE_HAL_H
#define _APLUS_CORE_HAL_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>


#define ARCH_VMM_AREA_HEAP          1
#define ARCH_VMM_AREA_KERNEL        2
#define ARCH_VMM_AREA_USER          3

#define ARCH_REBOOT_RESTART         0
#define ARCH_REBOOT_SUSPEND         1
#define ARCH_REBOOT_POWEROFF        2
#define ARCH_REBOOT_HALT            3


__BEGIN_DECLS

//* Virtual Memory
uintptr_t arch_vmm_getpagesize();
uintptr_t arch_vmm_gethugepagesize();
uintptr_t arch_vmm_p2v(uintptr_t, int);
uintptr_t arch_vmm_v2p(uintptr_t, int);
uintptr_t arch_vmm_map(vmm_address_space_t*, uintptr_t, uintptr_t, size_t, int);
uintptr_t arch_vmm_unmap(vmm_address_space_t*, uintptr_t, size_t);

//* Debug
void arch_debug_init(void);
void arch_debug_putc(char);

//* Timer
void arch_timer_delay(uint64_t);
uint64_t arch_timer_getticks(void);
uint64_t arch_timer_getus(void);
uint64_t arch_timer_getms(void);
uint64_t arch_timer_gettime(void);

//* Reboot
void arch_reboot(int);

__END_DECLS

#endif
#endif