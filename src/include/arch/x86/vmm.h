#ifndef _APLUS_X86_VMM_H
#define _APLUS_X86_VMM_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>



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
#define X86_MMU_PG_AP_TP_STACK      (1ULL << 10)
#define X86_MMU_PG_AP_TP_MMAP       (2ULL << 10)
#define X86_MMU_PG_AP_TP_SWAP       (3ULL << 10)

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
__END_DECLS

#endif
#endif