/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#ifndef _PAGING_H
#define _PAGING_H

#include <aplus.h>

#define X86_64_PT_ENTRIES                              512
#define X86_64_PD_ENTRIES                              512

#define X86_64_PAGE_SIZE                               4096


#define X86_64_FLAG_PRESENT                            1
#define X86_64_FLAG_RDWR                               2
#define X86_64_FLAG_USER                               4


typedef struct {
    uintptr_t flags:12;
    uintptr_t entry:20;
} __packed frame_t;

typedef struct {
    frame_t frames[X86_64_PT_ENTRIES];
} __packed pt_t;

typedef struct pdt {
    struct {
        uintptr_t flags:12;
        uintptr_t frame:20;
        
        pt_t* vframes[X86_64_PD_ENTRIES];
    } __packed pml4t[X86_64_PD_ENTRIES];

    struct {
        uintptr_t flags:12;
        uintptr_t frame:20;

        pt_t* vframes[X86_64_PD_ENTRIES];
    } __packed pdpt[X86_64_PD_ENTRIES];

    struct {
        uintptr_t flags:12;
        uintptr_t frame:20;
    } __packed pages[X86_64_PD_ENTRIES];

    pt_t* vpages[X86_64_PD_ENTRIES];

    uintptr_t physaddr;
    int refcount;

    volatile struct pdt* next;
} __packed pdt_t;


extern volatile pdt_t* current_pdt;
extern volatile pdt_t* kernel_pdt;

volatile pdt_t* vmm_clone(volatile pdt_t*, int);
void __vmmcpy(physaddr_t, physaddr_t);

volatile pdt_t* vmm_switch(volatile pdt_t*);
void vmm_release(volatile pdt_t* context);

#endif
