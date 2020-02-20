#ifndef _APLUS_CORE_MEMORY_H
#define _APLUS_CORE_MEMORY_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>

#define PML2_PAGESIZE               (128 * 1024 * 1024)     //? 128MiB
#define PML1_PAGESIZE               (4096)                  //? 4KiB

#define PML2_MAX_ENTRIES            (4096)
#define PML1_MAX_ENTRIES            (4096 / sizeof(uint64_t))


#define GFP_KERNEL                  0
#define GFP_ATOMIC                  1
#define GFP_USER                    2



__BEGIN_DECLS

void pmm_claim_area(uintptr_t, size_t);
void pmm_unclaim_area(uintptr_t, size_t);
uintptr_t pmm_alloc_block();
uintptr_t pmm_alloc_blocks(size_t);
void pmm_free_block(uintptr_t);
void pmm_free_blocks(uintptr_t, size_t);
void pmm_init(uintptr_t);


void* kmalloc(size_t, int);
void* kcalloc(size_t, size_t, int);
void* krealloc(void*, size_t, int);
void kfree(void*);

__END_DECLS

#endif
#endif