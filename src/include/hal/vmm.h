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


#ifndef R_OK
#define R_OK                        1
#endif

#ifndef W_OK
#define W_OK                        2
#endif

#ifndef X_OK
#define X_OK                        4
#endif


#define ptr_check(p, m)    \
    (arch_vmm_access(current_task->address_space, (uintptr_t) (p), (int) (m)) == 0 ? 1 : 0)


__BEGIN_DECLS

uintptr_t arch_vmm_getpagesize();
uintptr_t arch_vmm_gethugepagesize();
uintptr_t arch_vmm_p2v(uintptr_t, int);
uintptr_t arch_vmm_v2p(uintptr_t, int);
uintptr_t arch_vmm_map(vmm_address_space_t*, uintptr_t, uintptr_t, size_t, int);
uintptr_t arch_vmm_unmap(vmm_address_space_t*, uintptr_t, size_t);
uintptr_t arch_vmm_mprotect(vmm_address_space_t*, uintptr_t, size_t, int);
int arch_vmm_access(vmm_address_space_t*, uintptr_t, int);
uintptr_t arch_vmm_getphysaddr(vmm_address_space_t*, uintptr_t);
void arch_vmm_clone(vmm_address_space_t*, vmm_address_space_t*);

__END_DECLS

#endif
#endif