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


__END_DECLS

#endif
#endif