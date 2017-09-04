#ifndef _PAGING_H
#define _PAGING_H

#include <aplus.h>

#define X86_PT_ENTRIES                              1024
#define X86_PD_ENTRIES                              1024

#define X86_PAGE_SIZE                               4096


#define X86_FLAG_PRESENT                            1
#define X86_FLAG_RDWR                               2
#define X86_FLAG_USER                               4


typedef struct {
    uintptr_t flags:12;
    uintptr_t entry:20;
} __packed frame_t;

typedef struct {
    frame_t frames[X86_PT_ENTRIES];
} __packed pt_t;

typedef struct pdt {
    struct {
        uintptr_t flags:12;
        uintptr_t frame:20;
    } __packed pages[X86_PD_ENTRIES];

    pt_t* vpages[X86_PD_ENTRIES];

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
