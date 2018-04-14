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
    __asm__ __volatile__ ("mov rax, cr3; mov cr3, rax");


extern int PML4T;
volatile pdt_t* kernel_pdt = (volatile pdt_t*) &PML4T;
volatile pdt_t* current_pdt = (volatile pdt_t*) &PML4T;


static virtaddr_t __get_free_pages(volatile pdt_t* pdt, int count, int maked, int user) {
    KASSERT(count < X86_64_PT_ENTRIES);


    long f = 0;
    long i, j;
    for(i = 0; i < X86_64_PD_ENTRIES; i++) {
        if(unlikely(pdt->pages[i].frame == 0)) {
            if(likely(maked))
                continue;
            else
                return (virtaddr_t) (i << 22);
        }

        if(unlikely(pdt->vpages[i] == NULL))
            continue;

        if(unlikely(user))
            if(unlikely(!(pdt->pages[i].flags & X86_FLAG_USER)))
                continue;
        
        for(j = 0; j < X86_PT_ENTRIES - !(maked); j++) {
            if(pdt->vpages[i]->frames[j].flags & X86_FLAG_PRESENT) {
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



virtaddr_t get_free_pages(int count, int maked, int user) {
    virtaddr_t p = __get_free_pages(current_pdt, count, maked, user);
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
    if(--src->refcount)
        return;

    
    INTR_OFF;

    long i, j;
    for(i = 0; i < X86_PD_ENTRIES; i++) {
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

                //pmm_free_frame(src->vpages[i]->frames[j].entry);
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