#ifndef _APLUS_HAL_USERSPACE_H
#define _APLUS_HAL_USERSPACE_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>
#include <aplus/debug.h>

__BEGIN_DECLS

void arch_userspace_enter(uintptr_t, uintptr_t, void*);

__END_DECLS

#endif
#endif