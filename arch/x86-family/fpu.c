/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
                                                                        
#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/fpu.h>



//!
//! pmm_block(0) 
//!     + kmalloc(16) 
//!     + padding(48) = 64 bytes alignment
//!
#define FPU_PAD_SIZE        48



static void (*__fpu_switch) (void*, void*) = NULL;
static void (*__fpu_save) (void*) = NULL;
static void (*__fpu_restore) (void*) = NULL;

static uint8_t  __fpu_inital_state[8192] __attribute__((aligned(64))) = { 0 };
static uint16_t __fpu_size = 0;




static void xsaveopt_switch(void* prev, void* next) {

    DEBUG_ASSERT(prev);
    DEBUG_ASSERT(next);
    DEBUG_ASSERT(((uintptr_t) prev & 63) == 0);
    DEBUG_ASSERT(((uintptr_t) next & 63) == 0);


    __asm__ __volatile__ (
        "xsaveopt (%0)"
        :
        : "r"(prev), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
    );

    __asm__ __volatile__ (
        "xrstor (%0)"
        :
        : "r"(next), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
    );

}


static void xsave_switch(void* prev, void* next) {

    DEBUG_ASSERT(prev);
    DEBUG_ASSERT(next);
    DEBUG_ASSERT(((uintptr_t) prev & 63) == 0);
    DEBUG_ASSERT(((uintptr_t) next & 63) == 0);


    __asm__ __volatile__ (
        "xsave (%0)"
        :
        : "r"(prev), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
    );

    __asm__ __volatile__ (
        "xrstor (%0)"
        :
        : "r"(next), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
    );

}


static void xsave_save(void* fpu_area) {

    DEBUG_ASSERT(((uintptr_t) fpu_area & 63) == 0);

    __asm__ __volatile__ (
        "xsave (%0)"
        :
        : "r"(fpu_area), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
    );

}


static void xsave_restore(void* fpu_area) {

    DEBUG_ASSERT(((uintptr_t) fpu_area & 63) == 0);

    __asm__ __volatile__ (
        "xrstor (%0)"
        :
        : "r"(fpu_area), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
    );
    
}





static void fxsave_switch(void* prev, void* next) {

    DEBUG_ASSERT(((uintptr_t) prev & 15) == 0);
    DEBUG_ASSERT(((uintptr_t) next & 15) == 0);

    __asm__ __volatile__ (
        "fxsave (%0)"
        :
        : "r"(prev)
    );

    __asm__ __volatile__ (
        "fxrstor (%0)"
        :
        : "r"(next)
    );

}

static void fxsave_save(void* fpu_area) {

    DEBUG_ASSERT(((uintptr_t) fpu_area & 15) == 0);

    __asm__ __volatile__ (
        "fxsave (%0)"
        :
        : "r"(fpu_area)
    );

}


static void fxsave_restore(void* fpu_area) {

    DEBUG_ASSERT(((uintptr_t) fpu_area & 15) == 0);

    __asm__ __volatile__ (
        "fxrstor (%0)"
        :
        : "r"(fpu_area)
    );
    
}





static void fsave_switch(void* prev, void* next) {

    DEBUG_ASSERT(((uintptr_t) prev & 15) == 0);
    DEBUG_ASSERT(((uintptr_t) next & 15) == 0);

    __asm__ __volatile__ (
        "fsave (%0)"
        :
        : "r"(prev)
    );

    __asm__ __volatile__ (
        "frstor (%0)"
        :
        : "r"(next)
    );

}


static void fsave_save(void* fpu_area) {

    DEBUG_ASSERT(((uintptr_t) fpu_area & 15) == 0);

    __asm__ __volatile__ (
        "fsave (%0)"
        :
        : "r"(fpu_area)
    );

}


static void fsave_restore(void* fpu_area) {

    DEBUG_ASSERT(((uintptr_t) fpu_area & 15) == 0);

    __asm__ __volatile__ (
        "frstor (%0)"
        :
        : "r"(fpu_area)
    );
    
}






void fpu_init(uint64_t cpu) {

    if(!(boot_cpu_has(X86_FEATURE_FPU)))
        kpanicf("x86-fpu: FPU not supported by cpu, required!\n");

    DEBUG_ASSERT(((uintptr_t) &__fpu_inital_state[0] & 15) == 0);



    __asm__ __volatile__ (
        "fninit"
    );


    if(cpu_has(cpu, X86_FEATURE_XMM)) {

        x86_set_cr4(x86_get_cr4() | X86_CR4_OSFXSR_MASK);
        x86_set_cr4(x86_get_cr4() | X86_CR4_OSXMMEXCPT_MASK);

        x86_set_cr0(x86_get_cr0() & ~X86_CR0_EM_MASK);
        x86_set_cr0(x86_get_cr0() |  X86_CR0_MP_MASK);

    }


    if(cpu_has(cpu, X86_FEATURE_XSAVE)) {

        x86_set_cr4(x86_get_cr4() | X86_CR4_OSXSAVE_MASK);

    }





    if(cpu == SMP_CPU_BOOTSTRAP_ID) {


        if(boot_cpu_has(X86_FEATURE_XSAVE)) {
            
            uint64_t xcr0 = 0;
            
            #define XCR0_FPU        (1 << 0)
            #define XCR0_SSE        (1 << 1)
            #define XCR0_AVX        (1 << 2)
            #define XCR0_OPMASK     (1 << 5)
            #define XCR0_ZMM        (1 << 6)
            #define XCR0_ZMM2       (1 << 7)


            xcr0 |= XCR0_FPU;
            xcr0 |= XCR0_SSE;

            if(boot_cpu_has(X86_FEATURE_AVX))
                xcr0 |= XCR0_AVX;

            if(boot_cpu_has(X86_FEATURE_AVX512F))
                xcr0 |= XCR0_OPMASK | XCR0_ZMM | XCR0_ZMM2;


            x86_xsetbv(0, xcr0);


            long a, b, c, d;
            x86_cpuid(0xD, &a, &b, &c, &d);

            if(unlikely(!c))
                kpanicf("x86-fpu: cannot get size of FPU Extended Area\n");
            


            if(boot_cpu_has(X86_FEATURE_XSAVEOPT))
                __fpu_switch  = &xsaveopt_switch;
            else
                __fpu_switch  = &xsave_switch;


            __fpu_save    = &xsave_save;
            __fpu_restore = &xsave_restore;
            __fpu_size    = c;



#if defined(DEBUG) && DEBUG_LEVEL >= 4
            kprintf("x86-fpu: %s/XRSTOR feature set with %d bytes\n", boot_cpu_has(X86_FEATURE_XSAVEOPT)
                                                                        ? "XSAVEOPT"
                                                                        : "XSAVE"   , __fpu_size);
#endif

        }

        else if(boot_cpu_has(X86_FEATURE_FXSR)) {
            
            __fpu_switch  = &fxsave_switch;
            __fpu_save    = &fxsave_save;
            __fpu_restore = &fxsave_restore;
            __fpu_size    = 512;


#if defined(DEBUG) && DEBUG_LEVEL >= 4
            kprintf("x86-fpu: uses FXSAVE/FXRSTOR feature set with %d bytes\n", __fpu_size);
#endif
       
        }

        else {

            __fpu_switch  = &fsave_switch;
            __fpu_save    = &fsave_save;
            __fpu_restore = &fsave_restore;
            __fpu_size    = 108;


#if defined(DEBUG) && DEBUG_LEVEL >= 4
            kprintf("x86-fpu: uses FSAVE/FRSTOR feature set with %d bytes\n", __fpu_size);
#endif

        }




        fpu_save(&__fpu_inital_state[0]);

    }

}



void fpu_switch(void* prev, void* next) {

    DEBUG_ASSERT(__fpu_switch);

    return __fpu_switch(prev, next);
}


void fpu_save(void* fpu_area) {

    DEBUG_ASSERT(__fpu_save);

    return __fpu_save(fpu_area);
}


void fpu_restore(void* fpu_area) {

    DEBUG_ASSERT(__fpu_restore);

    return __fpu_restore(fpu_area);
}


size_t fpu_size(void) {

    DEBUG_ASSERT(__fpu_size);
    
    return __fpu_size;
}


void* fpu_new_state(void) {

    void* p = (void*) ((uintptr_t) kcalloc(fpu_size() + FPU_PAD_SIZE, 1, GFP_KERNEL) + FPU_PAD_SIZE);
    
    DEBUG_ASSERT( (uintptr_t) p);
    DEBUG_ASSERT(((uintptr_t) p & 63) == 0);


    memcpy(p, &__fpu_inital_state, fpu_size());

    return p;

}

void fpu_free_state(void* fpu_area) {

    DEBUG_ASSERT(fpu_area);

    return kfree((void*) ((uintptr_t) fpu_area - FPU_PAD_SIZE));
}