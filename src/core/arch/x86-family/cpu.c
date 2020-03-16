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
#include <aplus.h>
#include <aplus/multiboot.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>

#include <hal/cpu.h>
#include <hal/interrupt.h>
#include <hal/vmm.h>
#include <hal/timer.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/apic.h>
#include <arch/x86/smp.h>
#include <arch/x86/vmm.h>


__percpu
void arch_cpu_init(int index) {

    __builtin_cpu_init();


    core->cpu.cores[index].address_space.pm = x86_get_cr3();
    core->cpu.cores[index].address_space.size = 0;
    core->cpu.cores[index].address_space.refcount = 0;
    spinlock_init(&core->cpu.cores[index].address_space.lock);


    int i;
    for(i = 0; i < SMP_CPU_MAX_FEATURES; i++)
        core->cpu.cores[index].features[i] = 0ULL;
        




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

        core->cpu.cores[index].features[FEAT_1_EDX] = dx;
        core->cpu.cores[index].features[FEAT_1_ECX] = cx;


#if defined(DEBUG)

        kprintf("cpu: id:         #%d\n", index);
        kprintf("     vendor:     %s\n", vendor);
        kprintf("     features:   ");
        
        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_1_ECX] & p) kprintf("%s ", &(#p)[14])

        _F(X86_CPUID_EXT_SSE3);
        _F(X86_CPUID_EXT_PCLMULQDQ);
        _F(X86_CPUID_EXT_DTES64);
        _F(X86_CPUID_EXT_MONITOR);
        _F(X86_CPUID_EXT_DSCPL);
        _F(X86_CPUID_EXT_VMX);
        _F(X86_CPUID_EXT_SMX);
        _F(X86_CPUID_EXT_EST);
        _F(X86_CPUID_EXT_TM2);
        _F(X86_CPUID_EXT_SSSE3);
        _F(X86_CPUID_EXT_CID);
        _F(X86_CPUID_EXT_FMA);
        _F(X86_CPUID_EXT_CX16);
        _F(X86_CPUID_EXT_XTPR);
        _F(X86_CPUID_EXT_PDCM);
        _F(X86_CPUID_EXT_PCID);
        _F(X86_CPUID_EXT_DCA);
        _F(X86_CPUID_EXT_SSE41);
        _F(X86_CPUID_EXT_SSE42);
        _F(X86_CPUID_EXT_X2APIC);
        _F(X86_CPUID_EXT_MOVBE);
        _F(X86_CPUID_EXT_POPCNT);
        _F(X86_CPUID_EXT_TSC_DEADLINE_TIMER);
        _F(X86_CPUID_EXT_AES);
        _F(X86_CPUID_EXT_XSAVE);
        _F(X86_CPUID_EXT_OSXSAVE);
        _F(X86_CPUID_EXT_AVX);
        _F(X86_CPUID_EXT_F16C);
        _F(X86_CPUID_EXT_RDRAND);
        _F(X86_CPUID_EXT_HYPERVISOR);


        #undef _F
        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_1_EDX] & p) kprintf("%s ", &(#p)[10])


        _F(X86_CPUID_FP87);
        _F(X86_CPUID_VME);
        _F(X86_CPUID_DE);
        _F(X86_CPUID_PSE);
        _F(X86_CPUID_TSC);
        _F(X86_CPUID_MSR);
        _F(X86_CPUID_PAE);
        _F(X86_CPUID_MCE);
        _F(X86_CPUID_CX8);
        _F(X86_CPUID_APIC);
        _F(X86_CPUID_SEP);
        _F(X86_CPUID_MTRR);
        _F(X86_CPUID_PGE);
        _F(X86_CPUID_MCA);
        _F(X86_CPUID_CMOV);
        _F(X86_CPUID_PAT);
        _F(X86_CPUID_PSE36);
        _F(X86_CPUID_PN);
        _F(X86_CPUID_CLFLUSH);
        _F(X86_CPUID_DTS);
        _F(X86_CPUID_ACPI);
        _F(X86_CPUID_MMX);
        _F(X86_CPUID_FXSR);
        _F(X86_CPUID_SSE);
        _F(X86_CPUID_SSE2);
        _F(X86_CPUID_SS);
        _F(X86_CPUID_HT);
        _F(X86_CPUID_TM);
        _F(X86_CPUID_IA64);
        _F(X86_CPUID_PBE);

        #undef _F

#endif

    }



    if(ex >= 7) {
        
        cx = 0;
        x86_cpuid_extended(7, &ax, &bx, &cx, &dx);
        
        core->cpu.cores[index].features[FEAT_7_0_EBX] = bx;
        core->cpu.cores[index].features[FEAT_7_0_ECX] = cx;
        core->cpu.cores[index].features[FEAT_7_0_EDX] = dx;

#if defined(DEBUG)

        
        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_7_0_EBX] & p) kprintf("%s ", &(#p)[18])


        _F(X86_CPUID_7_0_EBX_FSGSBASE);
        _F(X86_CPUID_7_0_EBX_BMI1);
        _F(X86_CPUID_7_0_EBX_HLE);
        _F(X86_CPUID_7_0_EBX_AVX2);
        _F(X86_CPUID_7_0_EBX_SMEP);
        _F(X86_CPUID_7_0_EBX_BMI2);
        _F(X86_CPUID_7_0_EBX_ERMS);
        _F(X86_CPUID_7_0_EBX_INVPCID);
        _F(X86_CPUID_7_0_EBX_RTM);
        _F(X86_CPUID_7_0_EBX_MPX);
        _F(X86_CPUID_7_0_EBX_AVX512F);
        _F(X86_CPUID_7_0_EBX_AVX512DQ);
        _F(X86_CPUID_7_0_EBX_RDSEED);
        _F(X86_CPUID_7_0_EBX_ADX);
        _F(X86_CPUID_7_0_EBX_SMAP);
        _F(X86_CPUID_7_0_EBX_AVX512IFMA);
        _F(X86_CPUID_7_0_EBX_PCOMMIT);
        _F(X86_CPUID_7_0_EBX_CLFLUSHOPT);
        _F(X86_CPUID_7_0_EBX_CLWB);
        _F(X86_CPUID_7_0_EBX_INTEL_PT);
        _F(X86_CPUID_7_0_EBX_AVX512PF);
        _F(X86_CPUID_7_0_EBX_AVX512ER);
        _F(X86_CPUID_7_0_EBX_AVX512CD);
        _F(X86_CPUID_7_0_EBX_SHA_NI);
        _F(X86_CPUID_7_0_EBX_AVX512BW);
        _F(X86_CPUID_7_0_EBX_AVX512VL);


        #undef _F
        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_7_0_ECX] & p) kprintf("%s ", &(#p)[18])

        _F(X86_CPUID_7_0_ECX_AVX512_VBMI);
        _F(X86_CPUID_7_0_ECX_UMIP);
        _F(X86_CPUID_7_0_ECX_PKU);
        _F(X86_CPUID_7_0_ECX_OSPKE);
        _F(X86_CPUID_7_0_ECX_WAITPKG);
        _F(X86_CPUID_7_0_ECX_AVX512_VBMI2);
        _F(X86_CPUID_7_0_ECX_GFNI);
        _F(X86_CPUID_7_0_ECX_VAES);
        _F(X86_CPUID_7_0_ECX_VPCLMULQDQ);
        _F(X86_CPUID_7_0_ECX_AVX512VNNI);
        _F(X86_CPUID_7_0_ECX_AVX512BITALG);
        _F(X86_CPUID_7_0_ECX_AVX512_VPOPCNTDQ);
        _F(X86_CPUID_7_0_ECX_LA57);
        _F(X86_CPUID_7_0_ECX_RDPID);
        _F(X86_CPUID_7_0_ECX_CLDEMOTE);
        _F(X86_CPUID_7_0_ECX_MOVDIRI);
        _F(X86_CPUID_7_0_ECX_MOVDIR64B);


        #undef _F
        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_7_0_EDX] & p) kprintf("%s ", &(#p)[18])

        _F(X86_CPUID_7_0_EDX_AVX512_4VNNIW);
        _F(X86_CPUID_7_0_EDX_AVX512_4FMAPS);
        _F(X86_CPUID_7_0_EDX_SPEC_CTRL);
        _F(X86_CPUID_7_0_EDX_STIBP);
        _F(X86_CPUID_7_0_EDX_ARCH_CAPABILITIES);
        _F(X86_CPUID_7_0_EDX_CORE_CAPABILITY);
        _F(X86_CPUID_7_0_EDX_SPEC_CTRL_SSBD);
        

        #undef _F

#endif



        cx = 1;
        x86_cpuid_extended(7, &ax, &bx, &cx, &dx);

        core->cpu.cores[index].features[FEAT_7_1_EAX] = ax;


#if defined(DEBUG)

        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_7_1_EAX] & p) kprintf("%s ", &(#p)[18])
 
        _F(X86_CPUID_7_1_EAX_AVX512_BF16);


        #undef _F

#endif

    }




    x86_cpuid(0x80000000, &ex, &bx, &cx, &dx);


    if(ex >= 0x80000001) {

        x86_cpuid(0x80000001, &ax, &bx, &cx, &dx);

        core->cpu.cores[index].features[FEAT_8000_0001_EDX] = dx;
        core->cpu.cores[index].features[FEAT_8000_0001_ECX] = cx;


#if defined(DEBUG)
        
        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_8000_0001_EDX] & p) kprintf("%s ", &(#p)[15])

        _F(X86_CPUID_EXT2_SYSCALL);
        _F(X86_CPUID_EXT2_NX);
        _F(X86_CPUID_EXT2_FFXSR);
        _F(X86_CPUID_EXT2_PDPE1GB);
        _F(X86_CPUID_EXT2_RDTSCP);
        _F(X86_CPUID_EXT2_LM);

        #undef _F
        
#endif

    }


    if(ex >= 0x80000007) {

        x86_cpuid(0x80000007, &ax, &bx, &cx, &dx);

        core->cpu.cores[index].features[FEAT_8000_0007_EDX] = dx;


#if defined(DEBUG)
        
        #define _F(p)   \
            if(core->cpu.cores[index].features[FEAT_8000_0007_EDX] & p) kprintf("%s ", &(#p)[10])

        _F(X86_CPUID_APM_INVTSC);

        #undef _F

#endif
    }




#if defined(DEBUG)

        kprintf("\n");
        kprintf("     msr-efer:   ");

        uint64_t efer = x86_rdmsr(X86_MSR_EFER);


        #define _F(p)   \
            if(efer & p) kprintf("%s ", &(#p)[13])

        _F(X86_MSR_EFER_SCE);
        _F(X86_MSR_EFER_LME);
        _F(X86_MSR_EFER_LMA);
        _F(X86_MSR_EFER_NXE);
        _F(X86_MSR_EFER_SVME);
        _F(X86_MSR_EFER_LMSLE);
        _F(X86_MSR_EFER_FFXSR);
        _F(X86_MSR_EFER_TCE);

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
    BUG_ON(core->cpu.cores[index].features[FEAT_1_EDX] & X86_CPUID_MSR);
    BUG_ON(core->cpu.cores[index].features[FEAT_1_EDX] & X86_CPUID_SSE);


#if defined(__x86_64__)
    BUG_ON(core->cpu.cores[index].features[FEAT_1_EDX] & X86_CPUID_FXSR);
    BUG_ON(core->cpu.cores[index].features[FEAT_8000_0001_EDX] & X86_CPUID_EXT2_PDPE1GB);
    BUG_ON(core->cpu.cores[index].features[FEAT_8000_0001_EDX] & X86_CPUID_EXT2_LM);
#endif


#if defined(CONFIG_HAVE_SMP)
    BUG_ON(core->cpu.cores[index].features[FEAT_8000_0001_EDX] & X86_CPUID_EXT2_RDTSCP);
#endif









#if defined(DEBUG) && DEBUG_LEVEL >= 1

    #define check_and_enable(type, feature, func)                   \
        if(core->cpu.cores[index].type & feature) {                 \
            func;                                                   \
            kprintf("cpu: #%d -> %s\n", index, #feature);           \
        }

#else

    #define check_and_enable(type, feature, func)       \
        if(core->cpu.cores[index].type & feature) {     \
            func;                                       \
        }

#endif



    //! Enable TSC Timer
    check_and_enable(features[FEAT_1_EDX], X86_CPUID_TSC,
        x86_set_cr4(x86_get_cr4() | X86_CR4_TSD_MASK));


    //! Enable NX bit
    check_and_enable(features[FEAT_8000_0001_EDX], X86_CPUID_EXT2_NX,
        x86_wrmsr(X86_MSR_EFER, x86_rdmsr(X86_MSR_EFER) | X86_MSR_EFER_NXE));


    //! Enable Syscall bit
    check_and_enable(features[FEAT_8000_0001_EDX], X86_CPUID_EXT2_SYSCALL,
        x86_wrmsr(X86_MSR_EFER, x86_rdmsr(X86_MSR_EFER) | X86_MSR_EFER_SCE));


    //! Enable FXSAVE, FXRSTOR
    check_and_enable(features[FEAT_8000_0001_EDX], X86_CPUID_EXT2_FXSR,
        x86_set_cr4(x86_get_cr4() | X86_CR4_OSFXSR_MASK));


    //! Enable XSAVE and Extended states
    check_and_enable(features[FEAT_1_ECX], X86_CPUID_EXT_XSAVE,
        x86_set_cr4(x86_get_cr4() | X86_CR4_OSXSAVE_MASK));


    //! Enable FSGSBASE instructions
    check_and_enable(features[FEAT_7_0_EBX], X86_CPUID_7_0_EBX_FSGSBASE,
        x86_set_cr4(x86_get_cr4() | X86_CR4_FSGSBASE_MASK));


    //! Enable SMEP
    check_and_enable(features[FEAT_7_0_EBX], X86_CPUID_7_0_EBX_SMEP,
        x86_set_cr4(x86_get_cr4() | X86_CR4_SMEP_MASK));


    //! Enable SMAP
    check_and_enable(features[FEAT_7_0_EBX], X86_CPUID_7_0_EBX_SMAP,
        x86_set_cr4(x86_get_cr4() | X86_CR4_SMAP_MASK));
        

    //! Write Processor ID
    check_and_enable(features[FEAT_8000_0001_EDX], X86_CPUID_EXT2_RDTSCP,
        x86_wrmsr(X86_MSR_TSC_AUX, index));



#if defined(__x86_64__)

    // * FIXME: Invalid Opcode
    // if(core->cpu.cores[index].xfeatures & X86_CPU_XFEATURES_FSGSBASE)
    //     x86_wrgsbase((uint64_t) &core->cpu.cores[index]);
    // else

    x86_wrmsr(X86_MSR_KERNELGSBASE, (uint64_t) &core->cpu.cores[index]);

#endif


#if defined(__x86_64__)

    if(core->cpu.cores[index].features[FEAT_8000_0001_EDX] & X86_CPUID_EXT2_SYSCALL) {

        extern void x86_syscall_handler();

        x86_wrmsr(X86_MSR_STAR, ((uint64_t) KERNEL_CS << 32ULL) | ((uint64_t) ((USER_CS - 8) | 3) << 48ULL));
        x86_wrmsr(X86_MSR_LSTAR, (uint64_t) &x86_syscall_handler);
        x86_wrmsr(X86_MSR_FMASK, (uint64_t) 0x200ULL);

    }

#endif

    core->cpu.cores[index].flags |= SMP_CPU_FLAGS_ENABLED;

}


__percpu
uint64_t arch_cpu_get_current_id(void) {

    uint64_t id;

#if defined(CONFIG_X86_HAVE_RDPID)
    __asm__ __volatile__ (
        "rdpid %0"
        : "=0"(id)
    );
#else
    __asm__ __volatile__ (
        "rdtscp"
        : "=c"(id)
    );
#endif

    return id;
}


void arch_cpu_startup(int index) {

    DEBUG_ASSERT(index != SMP_CPU_BOOTSTRAP_ID);

    BUG_ON(!(core->cpu.cores[index].flags & SMP_CPU_FLAGS_ENABLED));
    BUG_ON( (core->cpu.cores[index].flags & SMP_CPU_FLAGS_AVAILABLE));


#if defined(DEBUG) && DEBUG_LEVEL >= 0
    kprintf("x86-cpu: starting up core #%d\n", index);
#endif

    //* Clone Address Space
    arch_vmm_clone(&core->cpu.cores[index].address_space, &core->bsp.address_space);


    //* Map AP Startup Area
    arch_vmm_map (&core->cpu.cores[index].address_space, AP_BOOT_OFFSET, AP_BOOT_OFFSET, X86_MMU_PAGESIZE, 
        ARCH_VMM_MAP_FIXED | 
        ARCH_VMM_MAP_RDWR
    );

    //* Map AP Stack Area
    arch_vmm_map (&core->cpu.cores[index].address_space, KERNEL_STACK_AREA, -1, X86_MMU_HUGE_2MB_PAGESIZE,
        ARCH_VMM_MAP_HUGETLB    | 
        ARCH_VMM_MAP_HUGE_2MB   |
        ARCH_VMM_MAP_TYPE_STACK |
        ARCH_VMM_MAP_RDWR);





    ap_init();

    ap_get_header()->cpu = (uint64_t) index;
    ap_get_header()->cr3 = (uint64_t) core->cpu.cores[index].address_space.pm;




    /* INIT */
    if(apic_is_x2apic()) {

        x86_wrmsr(X86_X2APIC_REG_ICR, (core->cpu.cores[index].id << 32) | (5 << 8) | (1 << 14));

        while (((x86_rdmsr(X86_X2APIC_REG_ICR) >> 32 >> 12) & 1))
            __builtin_ia32_pause();

    } else {

        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI, core->cpu.cores[index].id << 24);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_LO, (5 << 8) | (1 << 14));

        while (((mmio_r32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI) >> 12) & 1))
            __builtin_ia32_pause();

    }

    arch_timer_delay(10000);


    /* SIPI */
    if(apic_is_x2apic()) {

        x86_wrmsr(X86_X2APIC_REG_ICR, (core->cpu.cores[index].id << 32) | ((AP_BOOT_OFFSET >> 12) & 0xFF) | (6 << 8) | (1 << 14));

    } else {

        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI, core->cpu.cores[index].id << 24);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_LO, ((AP_BOOT_OFFSET >> 12) & 0xFF) | (6 << 8) | (1 << 14));

    }

    arch_timer_delay(200);

    if(ap_check(index, 500000))     /* 500 ms */
        return;


    /* SIPI (x2) */
    if(apic_is_x2apic()) {

        x86_wrmsr(X86_X2APIC_REG_ICR, (core->cpu.cores[index].id << 32) | ((AP_BOOT_OFFSET >> 12) & 0xFF) | (6 << 8) | (1 << 14));

    } else {

        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_HI, core->cpu.cores[index].id << 24);
        mmio_w32(X86_APIC_BASE_ADDR + X86_APIC_REG_ICR_LO, ((AP_BOOT_OFFSET >> 12) & 0xFF) | (6 << 8) | (1 << 14));

    }

    arch_timer_delay(200);

    if(ap_check(index, 3000000))    /* 3 secs */
        return;


    kprintf("x86-cpu: FAIL! starting up CPU #%d: id(%d) flags(%d)\n", index, core->cpu.cores[index].id, core->cpu.cores[index].flags);

}
