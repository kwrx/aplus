/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _ARCH_X86_MM_H
#define _ARCH_X86_MM_H

#include <aplus.h>
#include <aplus/mm.h>
#include <stdint.h>


#ifndef PAGE_SIZE
#define PAGE_SIZE                           4096
#endif

#define X86_MMU_PG_P                        (1ULL << 0)
#define X86_MMU_PG_RW                       (1ULL << 1)
#define X86_MMU_PG_U                        (1ULL << 2)
#define X86_MMU_PG_CD                       (1ULL << 4)
#define X86_MMU_PG_PS                       (1ULL << 7)
#define X86_MMU_PG_G                        (1ULL << 8)
#define X86_MMU_PG_NX                       (1ULL << 63)

/* System defined 11-9 */
#define X86_MMU_PG_AP_PFB                   (1ULL << 9)

#define X86_MMU_CLEAR                       0x000
#define X86_DIRTY_ACCESS_MASK               0xF9F

#define X86_MMU_KERNEL  \
    (X86_MMU_PG_RW | X86_MMU_PG_P)

#define X86_MMU_USER    \
    (X86_MMU_PG_U | X86_MMU_PG_RW | X86_MMU_PG_P)



#ifdef ARCH_X86_64
typedef uint64_t x86_page_t;
#else
typedef uint32_t x86_page_t;
#endif

void x86_map_page(x86_page_t*, uintptr_t, block_t, uint64_t);
void x86_unmap_page(x86_page_t*, uintptr_t);
int x86_ptr_access(uintptr_t, int);


#endif