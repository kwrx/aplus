#ifndef _APLUS_CORE_MEMORY_H
#define _APLUS_CORE_MEMORY_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>

__BEGIN_DECLS

void pmm_claim_area(uintptr_t, size_t);
void pmm_unclaim_area(uintptr_t, size_t);
void pmm_init(uintptr_t);

__END_DECLS

#endif
#endif