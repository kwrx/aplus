#ifndef _APLUS_X86_VMM_H
#define _APLUS_X86_VMM_H

#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/debug.h>
#include <arch/x86/intr.h>



#define X86_MMU_PG_P                (1ULL << 0)
#define X86_MMU_PG_RW               (1ULL << 1)
#define X86_MMU_PG_U                (1ULL << 2)
#define X86_MMU_PG_CD               (1ULL << 4)
#define X86_MMU_PG_PS               (1ULL << 7)
#define X86_MMU_PG_G                (1ULL << 8)
#define X86_MMU_PG_PAT              (1ULL << 12)

#define X86_MMU_PT_PAT              (1ULL << 7)
#define X86_MMU_PT_NX               (1ULL << 63)
#define X86_MMU_PT_ENTRIES          (512)


/* System defined 11-9 */
#define X86_MMU_PG_AP_PFB           (1ULL << 9)

#define X86_MMU_PG_AP_TP_PAGE       (0ULL << 10)
#define X86_MMU_PG_AP_TP_MMAP       (1ULL << 10)
#define X86_MMU_PG_AP_TP_COW        (2ULL << 10)

#define X86_MMU_PG_AP_TP_MASK       (3ULL << 10)



#define X86_MMU_PAGESIZE            0x1000
#define X86_MMU_HUGE_2MB_PAGESIZE   0x200000
#define X86_MMU_HUGE_1GB_PAGESIZE   0x40000000

#define X86_MMU_CLEAR               0x0000000000000000ULL
#define X86_MMU_DIRTY_ACCESS_MASK   0x0000000000000F9FULL
#define X86_MMU_ADDRESS_MASK        0x0000FFFFFFFFF000ULL


#define X86_MMU_KERNEL      \
    (X86_MMU_PG_P | X86_MMU_PG_RW)

#define X86_MMU_USER        \
    (X86_MMU_PG_P | X86_MMU_PG_RW | X86_MMU_PG_U)





#if defined(__x86_64__)
typedef uint64_t x86_page_t;
#elif
typedef uint32_t x86_page_t;
#endif

__BEGIN_DECLS


static inline uintptr_t __alloc_page(uintptr_t pagesize, int zero) {

    DEBUG_ASSERT(pagesize);
    DEBUG_ASSERT(X86_MMU_PAGESIZE == PML1_PAGESIZE);


    uintptr_t p;

    if(likely(pagesize == X86_MMU_PAGESIZE))
        p = pmm_alloc_block();
    else
        p = pmm_alloc_blocks_aligned(pagesize >> 12, pagesize);

    
    if(likely(zero))
        memset((void*) arch_vmm_p2v(p, ARCH_VMM_AREA_HEAP), 0, pagesize);

    return p;
}



void pagefault_handle(interrupt_frame_t*, uintptr_t);

__END_DECLS

#endif
#endif