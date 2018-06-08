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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/intr.h>
#include <libc.h>

#include <arch/i386/i386.h>
#include "paging.h"


#define __invlpg(x)                                                                     \
    __asm__ __volatile__ ("invlpg [%0]" : : "r"(x))
#define __invlpg_all()                                                                  \
    __asm__ __volatile__ ("mov eax, cr3; mov cr3, eax");


extern int PDT;
volatile pdt_t* kernel_pdt = (volatile pdt_t*) &PDT;
volatile pdt_t* current_pdt = (volatile pdt_t*) &PDT;


static virtaddr_t __get_free_pages(volatile pdt_t* pdt, int count, int made, int user) {
    KASSERT(count < 1024);


    long f = 0;
    long i, j;
    for(i = 768; i < X86_PD_ENTRIES; i++) {
        if(unlikely(pdt->pages[i].frame == 0)) {
            if(likely(made))
                continue;
            else
                return (virtaddr_t) (i << 22);
        }

        if(unlikely(pdt->vpages[i] == NULL))
            continue;

        if(unlikely(user))
            if(unlikely(!(pdt->pages[i].flags & X86_FLAG_USER)))
                continue;
        
        for(j = 0; j < X86_PT_ENTRIES - !(made); j++) {
            if(pdt->vpages[i]->frames[j].flags & X86_FLAG_PRESENT ||
               pdt->vpages[i]->frames[j].entry != 0) {
                f = 0;
                continue;
            }
            
            f++;
            if(f >= count)
                return (virtaddr_t) ((i << 22) + ((j - f + 1) << 12));
        }
    }

    return (virtaddr_t) NULL;
}

static volatile pdt_t* __vmm_create(void) {
    volatile pdt_t* pdt = (volatile pdt_t*) kvalloc(sizeof(volatile pdt_t), GFP_KERNEL);
    memset((void*) pdt, 0, sizeof(volatile pdt_t));

    pdt->physaddr = (uintptr_t) V2P(pdt);
    pdt->refcount = 1;
    
    volatile pdt_t* tmp;
    for(tmp = kernel_pdt; tmp->next; tmp = tmp->next)
        ;

    tmp->next = pdt;
    return pdt;
}

static void __map_page(volatile pdt_t* pdt, virtaddr_t virtaddr, physaddr_t physaddr, int user) {

    register long vframe = virtaddr >> 12;


    if(unlikely(pdt->pages[vframe >> 10].frame == 0)) {
        pdt->vpages[vframe >> 10] = (pt_t*) kvalloc(PAGE_SIZE, GFP_KERNEL);
        memset(pdt->vpages[vframe >> 10], 0, PAGE_SIZE);

        pdt->pages[vframe >> 10].frame = V2P(pdt->vpages[vframe >> 10]) >> 12;
        pdt->pages[vframe >> 10].flags = X86_FLAG_PRESENT | X86_FLAG_RDWR;

        if(unlikely(user))
            pdt->pages[vframe >> 10].flags |= X86_FLAG_USER;


        long i;
        for(i = 0; i < X86_PT_ENTRIES; i++)
            pdt->vpages[vframe >> 10]->frames[i].entry =
            pdt->vpages[vframe >> 10]->frames[i].flags = 0;

    }

    
    pdt->vpages[vframe >> 10]->frames[vframe % 1024].entry = (physaddr >> 12);
    pdt->vpages[vframe >> 10]->frames[vframe % 1024].flags = X86_FLAG_PRESENT | X86_FLAG_RDWR;

    if(unlikely(user))
        pdt->vpages[vframe >> 10]->frames[vframe % 1024].flags |= X86_FLAG_USER;


    if(likely(current_pdt == pdt))    
        __invlpg(virtaddr);
}


static void __unmap_page(volatile pdt_t* pdt, virtaddr_t virtaddr) {
    register long vframe = virtaddr >> 12;


    //if(likely(pdt->vpages[vframe >> 10]->frames[vframe % 1024].entry))
    //    pmm_free_frame(pdt->vpages[vframe >> 10]->frames[vframe % 1024].entry);

    pdt->vpages[vframe >> 10]->frames[vframe % 1024].entry = 0;
    pdt->vpages[vframe >> 10]->frames[vframe % 1024].flags = 0;

    if(likely(current_pdt == pdt))    
        __invlpg(virtaddr);
}


static void __enable_page(volatile pdt_t* pdt, virtaddr_t virtaddr) {
    INTR_OFF;

    register long vframe = virtaddr >> 12;
    pdt->vpages[vframe >> 10]->frames[vframe % 1024].flags |= (X86_FLAG_PRESENT);

    if(likely(current_pdt == pdt))    
        __invlpg(virtaddr);

    INTR_ON;
}


static void __disable_page(volatile pdt_t* pdt, virtaddr_t virtaddr) {
    INTR_OFF;

    register long vframe = virtaddr >> 12;
    pdt->vpages[vframe >> 10]->frames[vframe % 1024].flags &= ~(X86_FLAG_PRESENT);

    if(likely(current_pdt == pdt))    
        __invlpg(virtaddr);

    INTR_ON;
}



void map_page(virtaddr_t virtaddr, physaddr_t physaddr, int user) {
    INTR_OFF;

    __map_page(current_pdt, virtaddr, physaddr, user);

    if(virtaddr > CONFIG_KERNEL_BASE) {

        volatile pdt_t* tmp;
        for(tmp = kernel_pdt; tmp; tmp = tmp->next)
            if(likely(tmp != current_pdt))
                 __map_page(tmp, virtaddr, physaddr, user);

    }

    INTR_ON;
}

void unmap_page(virtaddr_t virtaddr) {
    INTR_OFF;

    __unmap_page(current_pdt, virtaddr);


    if(virtaddr > CONFIG_KERNEL_BASE) {

        volatile pdt_t* tmp;
        for(tmp = kernel_pdt; tmp; tmp = tmp->next)
            if(likely(tmp != current_pdt))
                 __unmap_page(tmp, virtaddr);

    }

    INTR_ON;
}


void enable_page(virtaddr_t virtaddr) {
    INTR_OFF;

    __enable_page(current_pdt, virtaddr);


    if(virtaddr > CONFIG_KERNEL_BASE) {

        volatile pdt_t* tmp;
        for(tmp = kernel_pdt; tmp; tmp = tmp->next)
            if(likely(tmp != current_pdt))
                 __enable_page(tmp, virtaddr);

    }

    INTR_ON;
}



void disable_page(virtaddr_t virtaddr) {
    INTR_OFF;

    __disable_page(current_pdt, virtaddr);


    if(virtaddr > CONFIG_KERNEL_BASE) {

        volatile pdt_t* tmp;
        for(tmp = kernel_pdt; tmp; tmp = tmp->next)
            if(likely(tmp != current_pdt))
                 __disable_page(tmp, virtaddr);

    }

    INTR_ON;
}



virtaddr_t get_free_pages(int count, int made, int user) {
    virtaddr_t p = __get_free_pages(current_pdt, count, made, user);
    KASSERT(p);
    return p;
}


void __vmmcpy(physaddr_t dest, physaddr_t src) {    
    INTR_OFF;

    void *vd, *vs;
    
    __map_page(current_pdt, (virtaddr_t) (vd = (void*) get_free_pages(1, 1, 0)), dest, 0);
    __map_page(current_pdt, (virtaddr_t) (vs = (void*) get_free_pages(1, 1, 0)), src, 0);

    KASSERT(vd && vs);

    memcpy(vd, vs, PAGE_SIZE);

    __unmap_page(current_pdt, (virtaddr_t) vd);
    __unmap_page(current_pdt, (virtaddr_t) vs);

    INTR_ON;
}

physaddr_t __V2P(virtaddr_t virtaddr) {
    register long vframe = virtaddr >> 12;

    if(unlikely(current_pdt->pages[vframe >> 10].frame == 0))
        return 0;

    if(unlikely(!(current_pdt->vpages[vframe >> 10]->frames[vframe % 1024].flags & X86_FLAG_PRESENT)))
        return 0;

    return (physaddr_t) (current_pdt->vpages[vframe >> 10]->frames[vframe % 1024].entry << 12) | (virtaddr & 0xFFF);
}



volatile pdt_t* vmm_clone(volatile pdt_t* src, int dup) {
    if(unlikely(!dup)) {
        src->refcount++;
        return src;    
    }
    
    
    INTR_OFF;


    volatile pdt_t* dest = __vmm_create();


    long i, j;
    for(i = 0; i < X86_PD_ENTRIES; i++) {
        if(likely(!(src->pages[i].flags & X86_FLAG_PRESENT)))
            continue;

        KASSERT(src->vpages[i]);

        if(unlikely(
            (kernel_pdt->vpages[i] == src->vpages[i])
            && (i != (CONFIG_STACK_BASE >> 22))
        )) {
            dest->vpages[i] = src->vpages[i];

            dest->pages[i].frame = src->pages[i].frame;
            dest->pages[i].flags = src->pages[i].flags;
        } else {

            dest->vpages[i] = (pt_t*) kvalloc(PAGE_SIZE, GFP_KERNEL);

            dest->pages[i].frame = V2P(dest->vpages[i]) >> 12;
            dest->pages[i].flags = src->pages[i].flags;

            KASSERT(dest->vpages[i]);
            KASSERT(dest->pages[i].frame);


            for(j = 0; j < X86_PT_ENTRIES; j++) {
                if(!(src->vpages[i]->frames[j].flags & X86_FLAG_PRESENT))
                    continue;


                dest->vpages[i]->frames[j].entry = pmm_alloc_frame();
                dest->vpages[i]->frames[j].flags = src->vpages[i]->frames[j].flags;

                __vmmcpy(dest->vpages[i]->frames[j].entry << 12, src->vpages[i]->frames[j].entry << 12);
            }
        }
    }


    INTR_ON;
    return dest;
}

void vmm_release(volatile pdt_t* src) {
    INTR_OFF;

    long i, j;
    for(i = 0; i < (CONFIG_KERNEL_BASE >> 22); i++) {
        if(likely(!(src->pages[i].flags & X86_FLAG_PRESENT)))
            continue;

        KASSERT(src->vpages[i]);

        if(unlikely(
            (kernel_pdt->vpages[i] == src->vpages[i])
            && (i != (CONFIG_STACK_BASE >> 22))
        )) {
            continue;

        } else {

            if((i == (CONFIG_STACK_BASE >> 22)) && (current_pdt == src))
                continue;
    
            for(j = 0; j < X86_PT_ENTRIES; j++) {
                if(!(src->vpages[i]->frames[j].flags & X86_FLAG_PRESENT))
                    continue;

                pmm_free_frame(src->vpages[i]->frames[j].entry);
            }


            kfree(src->vpages[i]);
        }
    }

    INTR_ON;
}

volatile pdt_t* vmm_switch(volatile pdt_t* context) {
    volatile pdt_t* p = current_pdt;
    if(!context)
        return p;



    current_pdt = context;
    __invlpg_all();
    


    return p;
}


EXPORT(unmap_page);
EXPORT(map_page);
EXPORT(get_free_pages);
EXPORT(__V2P);
