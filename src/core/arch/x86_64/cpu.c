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
#include <string.h>
#include <aplus/core/base.h>
#include <aplus/core/multiboot.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>
#include <aplus/core/ipc.h>
#include <aplus/core/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>


__percpu
void arch_cpu_init(int index) {

    __builtin_cpu_init();


    core->cpu.cores[index].id = index;
    core->cpu.cores[index].features = 0ULL;
    core->cpu.cores[index].xfeatures = 0ULL;

    core->cpu.cores[index].address_space.pm = x86_get_cr3();
    core->cpu.cores[index].address_space.size = 0;
    core->cpu.cores[index].address_space.refcount = 0;
    spinlock_init(&core->cpu.cores[index].address_space.lock);


    long ax, bx, cx, dx;
    long ex;

    x86_cpuid(0, &ex, &bx, &cx, &dx);


#if defined(DEBUG)

    char vendor[13];
    vendor[12] = '\0';

    memcpy(&vendor[0], &bx, 4);
    memcpy(&vendor[4], &dx, 4);
    memcpy(&vendor[8], &cx, 4);

#endif

    if(ex >= 1) {

        x86_cpuid(1, &ax, &bx, &cx, &dx);

        core->cpu.cores[index].features = (dx << 32) | cx;


#if defined(DEBUG)

        kprintf("cpu: id:         #%d\n", index);
        kprintf("     vendor:     %s\n", vendor);
        kprintf("     features:   ");
        
        #define _F(p)   \
            if(core->cpu.cores[index].features & p) kprintf("%s ", &(#p)[17])

        _F(X86_CPU_FEATURES_SSE3);
        _F(X86_CPU_FEATURES_PCLMULQDQ);
        _F(X86_CPU_FEATURES_DTES64);
        _F(X86_CPU_FEATURES_MONITOR);
        _F(X86_CPU_FEATURES_DS_CPL);
        _F(X86_CPU_FEATURES_VMX);
        _F(X86_CPU_FEATURES_SMX);
        _F(X86_CPU_FEATURES_EST);
        _F(X86_CPU_FEATURES_TM2);
        _F(X86_CPU_FEATURES_SSSE3);
        _F(X86_CPU_FEATURES_CNXT_ID);
        _F(X86_CPU_FEATURES_FMA);
        _F(X86_CPU_FEATURES_CX16);
        _F(X86_CPU_FEATURES_XTPR);
        _F(X86_CPU_FEATURES_PDCM);
        _F(X86_CPU_FEATURES_PCID);
        _F(X86_CPU_FEATURES_DCA);
        _F(X86_CPU_FEATURES_SSE41);
        _F(X86_CPU_FEATURES_SSE42);
        _F(X86_CPU_FEATURES_X2APIC);
        _F(X86_CPU_FEATURES_MOVBE);
        _F(X86_CPU_FEATURES_POPCNT);
        _F(X86_CPU_FEATURES_TSC);
        _F(X86_CPU_FEATURES_AESNI);
        _F(X86_CPU_FEATURES_XSAVE);
        _F(X86_CPU_FEATURES_OSXSAVE);
        _F(X86_CPU_FEATURES_AVX);
        _F(X86_CPU_FEATURES_F16C);
        _F(X86_CPU_FEATURES_RDRAND);

        _F(X86_CPU_FEATURES_FPU);
        _F(X86_CPU_FEATURES_VME);
        _F(X86_CPU_FEATURES_DE);
        _F(X86_CPU_FEATURES_PSE);
        _F(X86_CPU_FEATURES_TSC);
        _F(X86_CPU_FEATURES_MSR);
        _F(X86_CPU_FEATURES_PAE);
        _F(X86_CPU_FEATURES_MCE);
        _F(X86_CPU_FEATURES_CX8);
        _F(X86_CPU_FEATURES_APIC);
        _F(X86_CPU_FEATURES_SEP);
        _F(X86_CPU_FEATURES_MTRR);
        _F(X86_CPU_FEATURES_PGE);
        _F(X86_CPU_FEATURES_MCA);
        _F(X86_CPU_FEATURES_CMOV);
        _F(X86_CPU_FEATURES_PAT);
        _F(X86_CPU_FEATURES_PSE36);
        _F(X86_CPU_FEATURES_PSN);
        _F(X86_CPU_FEATURES_CLFLUSH);
        _F(X86_CPU_FEATURES_DS);
        _F(X86_CPU_FEATURES_ACPI);
        _F(X86_CPU_FEATURES_MMX);
        _F(X86_CPU_FEATURES_FXSR);
        _F(X86_CPU_FEATURES_SSE);
        _F(X86_CPU_FEATURES_SSE2);
        _F(X86_CPU_FEATURES_SS);
        _F(X86_CPU_FEATURES_HTT);
        _F(X86_CPU_FEATURES_TM);
        _F(X86_CPU_FEATURES_PBE);

        #undef _F

        kprintf("\n");
#endif

    }


    x86_cpuid(0x80000000, &ex, &bx, &cx, &dx);


    if(ex >= 0x80000001) {

        x86_cpuid(0x80000001, &ax, &bx, &cx, &dx);

        core->cpu.cores[index].xfeatures = (dx << 32) | cx;


#if defined(DEBUG)
        
        kprintf("     xfeatures:  ");

        #define _F(p)   \
            if(core->cpu.cores[index].xfeatures & p) kprintf("%s ", &(#p)[18])

        _F(X86_CPU_XFEATURES_SYSCALL);
        _F(X86_CPU_XFEATURES_XD);
        _F(X86_CPU_XFEATURES_1GB_PAGE);
        _F(X86_CPU_XFEATURES_RDTSCP);
        _F(X86_CPU_XFEATURES_64_BIT);

        #undef _F

        kprintf("\n");
#endif

    }

#if defined(DEBUG)


        kprintf("     msr-efer:   ");

        uint64_t efer = x86_rdmsr(X86_MSR_EFER);


        #define _F(p)   \
            if(efer & p) kprintf("%s ", &(#p)[17])

        _F(X86_MSR_FEATURES_SCE);
        _F(X86_MSR_FEATURES_LME);
        _F(X86_MSR_FEATURES_LMA);
        _F(X86_MSR_FEATURES_NXE);
        _F(X86_MSR_FEATURES_SVME);
        _F(X86_MSR_FEATURES_LMSLE);
        _F(X86_MSR_FEATURES_FFXSR);
        _F(X86_MSR_FEATURES_TCE);

        #undef _F

        kprintf("\n");

    if(ex >= 0x80000004) {

        char name[48];
        x86_cpuid(0x80000002, (long *)(name +  0), (long *)(name +  4), (long *)(name +  8), (long *)(name + 12));
        x86_cpuid(0x80000003, (long *)(name + 16), (long *)(name + 20), (long *)(name + 24), (long *)(name + 28));
        x86_cpuid(0x80000004, (long *)(name + 32), (long *)(name + 36), (long *)(name + 40), (long *)(name + 44));

        const char* p = name;
        while(*p == ' ')
            p++;

        kprintf("     name:       %s\n", p);

    }

#endif




    //! Requirements
    BUG_ON(core->cpu.cores[index].features & X86_CPU_FEATURES_MSR);
    BUG_ON(core->cpu.cores[index].features & X86_CPU_FEATURES_SSE);
    //BUG_ON(core->cpu.cores[index].xfeatures & X86_CPU_XFEATURES_1GB_PAGE);     // FIXME
    BUG_ON(core->cpu.cores[index].xfeatures & X86_CPU_XFEATURES_64_BIT);



    //* Enable NX bit
    if(core->cpu.cores[index].xfeatures & X86_CPU_XFEATURES_XD)
        x86_wrmsr(X86_MSR_EFER, x86_rdmsr(X86_MSR_EFER) | X86_MSR_FEATURES_NXE);


    //* Enable Syscall bit
    if(core->cpu.cores[index].xfeatures & X86_CPU_XFEATURES_SYSCALL) 
        x86_wrmsr(X86_MSR_EFER, x86_rdmsr(X86_MSR_EFER) | X86_MSR_FEATURES_SCE);


    //* Enable Fast FXSAVE, FXRSTOR
    //if(core->cpu.cores[index].features & X86_CPU_FEATURES_XSAVE)
    //    x86_wrmsr(X86_MSR_EFER, x86_rdmsr(X86_MSR_EFER) | X86_MSR_FEATURES_FFXSR);


}



    