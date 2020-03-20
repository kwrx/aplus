#ifndef _APLUS_MEMORY_H
#define _APLUS_MEMORY_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>


//* PMM */
#define PML2_PAGESIZE               (128 * 1024 * 1024)         //? 128MiB
#define PML1_PAGESIZE               (4096)                      //? 4KiB

#define PML2_MAX_ENTRIES            (16384)                     //? 2TiB
#define PML1_MAX_ENTRIES            (4096 / sizeof(uint64_t))


//* Heap */
#define GFP_KERNEL                  0
#define GFP_ATOMIC                  1
#define GFP_USER                    2


//* MMIO */
#define mmio_r8(p)                  (*(uint8_t volatile*) (p))
#define mmio_r16(p)                 (*(uint16_t volatile*) (p))
#define mmio_r32(p)                 (*(uint32_t volatile*) (p))
#define mmio_r64(p)                 (*(uint64_t volatile*) (p))

#define mmio_w8(p, v)               { mmio_r8(p) = (uint8_t) (v); }
#define mmio_w16(p, v)              { mmio_r16(p) = (uint16_t) (v); }
#define mmio_w32(p, v)              { mmio_r32(p) = (uint32_t) (v); }
#define mmio_w64(p, v)              { mmio_r64(p) = (uint64_t) (v); }


//* Virtual Memory */
#define ARCH_VMM_MAP_RDWR           (1 << 0)
#define ARCH_VMM_MAP_NOEXEC         (1 << 1)
#define ARCH_VMM_MAP_USER           (1 << 2)
#define ARCH_VMM_MAP_UNCACHED       (1 << 3)
#define ARCH_VMM_MAP_SHARED         (1 << 4)
#define ARCH_VMM_MAP_FIXED          (1 << 5)
#define ARCH_VMM_MAP_DEMAND         (1 << 6)
#define ARCH_VMM_MAP_VIDEO_MEMORY   (1 << 7)

#define ARCH_VMM_MAP_HUGETLB        (1 << 7)
#define ARCH_VMM_MAP_HUGE_2MB       (0 << 8)
#define ARCH_VMM_MAP_HUGE_1GB       (1 << 8)

#define ARCH_VMM_MAP_TYPE_MASK      (3 << 9)
#define ARCH_VMM_MAP_TYPE_PAGE      (0 << 9)
#define ARCH_VMM_MAP_TYPE_UNIQUE    (1 << 9)
#define ARCH_VMM_MAP_TYPE_MMAP      (2 << 9)
#define ARCH_VMM_MAP_TYPE_COW       (3 << 9)



typedef struct vmm_address_space {
    
    uintptr_t pm;
    size_t size;
    int refcount;
    spinlock_t lock;

} vmm_address_space_t;



__BEGIN_DECLS

void pmm_claim_area(uintptr_t, size_t);
void pmm_unclaim_area(uintptr_t, size_t);
uintptr_t pmm_alloc_block();
uintptr_t pmm_alloc_blocks(size_t);
uintptr_t pmm_alloc_blocks_aligned(size_t, uintptr_t);
void pmm_free_block(uintptr_t);
void pmm_free_blocks(uintptr_t, size_t);
uint64_t pmm_get_used_memory();
void pmm_init(uintptr_t);


void* kmalloc(size_t, int);
void* kcalloc(size_t, size_t, int);
void* krealloc(void*, size_t, int);
void kfree(void*);

__END_DECLS

#endif
#endif