#ifndef _APLUS_HAL_VMM_H
#define _APLUS_HAL_VMM_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/task.h>


#define ARCH_VMM_AREA_HEAP          1
#define ARCH_VMM_AREA_KERNEL        2
#define ARCH_VMM_AREA_USER          3

__BEGIN_DECLS

uintptr_t arch_vmm_getpagesize();
uintptr_t arch_vmm_gethugepagesize();
uintptr_t arch_vmm_p2v(uintptr_t, int);
uintptr_t arch_vmm_v2p(uintptr_t, int);
uintptr_t arch_vmm_map(vmm_address_space_t*, uintptr_t, uintptr_t, size_t, int);
uintptr_t arch_vmm_unmap(vmm_address_space_t*, uintptr_t, size_t);
void arch_vmm_clone(vmm_address_space_t*, vmm_address_space_t*);

__END_DECLS

#endif
#endif