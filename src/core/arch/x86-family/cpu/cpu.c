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
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>
#include <aplus/hal.h>

#include <arch/x86/cpu.h>
#include <arch/x86/asm.h>
#include <arch/x86/intr.h>
#include <arch/x86/apic.h>
#include <arch/x86/smp.h>
#include <arch/x86/vmm.h>


// see startup.S
extern int startup_tss;



__percpu
void arch_cpu_init(int index) {

    __builtin_cpu_init();


    if(index == SMP_CPU_BOOTSTRAP_ID) {
        
        core->cpu.cores[index].address_space.pm = x86_get_cr3();
        core->cpu.cores[index].address_space.size = 0;
        core->cpu.cores[index].address_space.refcount = 1;
        spinlock_init(&core->cpu.cores[index].address_space.lock);

    }


    int i;
    for(i = 0; i < SMP_CPU_MAX_FEATURES; i++)
        core->cpu.cores[index].features[i] = 0ULL;
        
    core->cpu.cores[index].tss = (void*) (&startup_tss + (index * sizeof(tss_t)));


    spinlock_init(&core->cpu.cores[index].global_lock);
    spinlock_init(&core->cpu.cores[index].sched_lock);

    core->cpu.cores[index].sched_count = 0;




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

        core->cpu.cores[index].features[0] = dx;
        core->cpu.cores[index].features[4] = cx;

    }


    if(ex >= 7) {
        
        x86_cpuid(7, &ax, &bx, &cx, &dx);
        
        core->cpu.cores[index].features[9] = bx;
    
    }


    if(ex >= 13) {
        
        cx = 1;
        x86_cpuid_extended(13, &ax, &bx, &cx, &dx);
        
        core->cpu.cores[index].features[10] = ax;
    
    }


    if(ex >= 15) {
        
        cx = 0;
        x86_cpuid_extended(15, &ax, &bx, &cx, &dx);
        
        core->cpu.cores[index].features[11] = dx;


        cx = 1;
        x86_cpuid_extended(15, &ax, &bx, &cx, &dx);
        
        core->cpu.cores[index].features[12] = dx;
    
    }


    long eex;
    x86_cpuid(0x80000000, &eex, &bx, &cx, &dx);


    if(eex >= 0x80000001) {

        x86_cpuid(0x80000001, &ax, &bx, &cx, &dx);

        core->cpu.cores[index].features[1] = dx;
        core->cpu.cores[index].features[6] = cx;


    }


    if(eex >= 0x80860001) {

        x86_cpuid(0x80860001, &ax, &bx, &cx, &dx);

        core->cpu.cores[index].features[2] = dx;

    }





#if defined(DEBUG)

        kprintf("cpu: id:         #%d\n", index);
        kprintf("     vendor:     %s\n", vendor);
        kprintf("     cpuid:      %p (extended: %p)\n", ex, eex);
        kprintf("     features:   ");
        


        #define _F(p)   \
            if(cpu_has(index, p)) kprintf("%s ", &(#p)[12]);
            

        _F(X86_FEATURE_FPU                ) /* Onboard FPU */
        _F(X86_FEATURE_VME                ) /* Virtual Mode Extensions */
        _F(X86_FEATURE_DE                 ) /* Debugging Extensions */
        _F(X86_FEATURE_PSE                ) /* Page Size Extensions */
        _F(X86_FEATURE_TSC                ) /* Time Stamp Counter */
        _F(X86_FEATURE_MSR                ) /* Model-Specific Registers */
        _F(X86_FEATURE_PAE                ) /* Physical Address Extensions */
        _F(X86_FEATURE_MCE                ) /* Machine Check Exception */
        _F(X86_FEATURE_CX8                ) /* CMPXCHG8 instruction */
        _F(X86_FEATURE_APIC               ) /* Onboard APIC */
        _F(X86_FEATURE_SEP                ) /* SYSENTER/SYSEXIT */
        _F(X86_FEATURE_MTRR               ) /* Memory Type Range Registers */
        _F(X86_FEATURE_PGE                ) /* Page Global Enable */
        _F(X86_FEATURE_MCA                ) /* Machine Check Architecture */
        _F(X86_FEATURE_CMOV               ) /* CMOV instructions */
                                                       /* (plus FCMOVcc, FCOMI with FPU) */
        _F(X86_FEATURE_PAT                ) /* Page Attribute Table */
        _F(X86_FEATURE_PSE36              ) /* 36-bit PSEs */
        _F(X86_FEATURE_PN                 ) /* Processor serial number */
        _F(X86_FEATURE_CLFLUSH            ) /* CLFLUSH instruction */
        _F(X86_FEATURE_DS                 ) /* "dts" Debug Store */
        _F(X86_FEATURE_ACPI               ) /* ACPI via MSR */
        _F(X86_FEATURE_MMX                ) /* Multimedia Extensions */
        _F(X86_FEATURE_FXSR               ) /* FXSAVE/FXRSTOR, CR4.OSFXSR */
        _F(X86_FEATURE_XMM                ) /* "sse" */
        _F(X86_FEATURE_XMM2               ) /* "sse2" */
        _F(X86_FEATURE_SELFSNOOP          ) /* "ss" CPU self snoop */
        _F(X86_FEATURE_HT                 ) /* Hyper-Threading */
        _F(X86_FEATURE_ACC                ) /* "tm" Automatic clock control */
        _F(X86_FEATURE_IA64               ) /* IA-64 processor */
        _F(X86_FEATURE_PBE                ) /* Pending Break Enable */

        _F(X86_FEATURE_SYSCALL            ) /* SYSCALL/SYSRET */
        _F(X86_FEATURE_MP                 ) /* MP Capable. */
        _F(X86_FEATURE_NX                 ) /* Execute Disable */
        _F(X86_FEATURE_MMXEXT             ) /* AMD MMX extensions */
        _F(X86_FEATURE_FXSR_OPT           ) /* FXSAVE/FXRSTOR optimizations */
        _F(X86_FEATURE_GBPAGES            ) /* "pdpe1gb" GB pages */
        _F(X86_FEATURE_RDTSCP             ) /* RDTSCP */
        _F(X86_FEATURE_LM                 ) /* Long Mode (x86-64) */
        _F(X86_FEATURE_3DNOWEXT           ) /* AMD 3DNow! extensions */
        _F(X86_FEATURE_3DNOW              ) /* 3DNow! */

        _F(X86_FEATURE_RECOVERY           ) /* CPU in recovery mode */
        _F(X86_FEATURE_LONGRUN            ) /* Longrun power control */
        _F(X86_FEATURE_LRTI               ) /* LongRun table interface */

        _F(X86_FEATURE_CONSTANT_TSC       ) /* TSC ticks at a constant rate */
        _F(X86_FEATURE_ARCH_PERFMON       ) /* Intel Architectural PerfMon */
        _F(X86_FEATURE_PEBS               ) /* Precise-Event Based Sampling */
        _F(X86_FEATURE_BTS                ) /* Branch Trace Store */
        _F(X86_FEATURE_MFENCE_RDTSC       ) /* "" Mfence synchronizes RDTSC */
        _F(X86_FEATURE_LFENCE_RDTSC       ) /* "" Lfence synchronizes RDTSC */
        _F(X86_FEATURE_NONSTOP_TSC        ) /* TSC does not stop in C states */
        _F(X86_FEATURE_APERFMPERF         ) /* APERFMPERF */
        _F(X86_FEATURE_NONSTOP_TSC_S3     ) /* TSC doesn't stop in S3 state */

        _F(X86_FEATURE_XMM3               ) /* "pni" SSE-3 */
        _F(X86_FEATURE_PCLMULQDQ          ) /* PCLMULQDQ instruction */
        _F(X86_FEATURE_DTES64             ) /* 64-bit Debug Store */
        _F(X86_FEATURE_MWAIT              ) /* "monitor" Monitor/Mwait support */
        _F(X86_FEATURE_DSCPL              ) /* "ds_cpl" CPL Qual. Debug Store */
        _F(X86_FEATURE_VMX                ) /* Hardware virtualization */
        _F(X86_FEATURE_SMX                ) /* Safer mode */
        _F(X86_FEATURE_EST                ) /* Enhanced SpeedStep */
        _F(X86_FEATURE_TM2                ) /* Thermal Monitor 2 */
        _F(X86_FEATURE_SSSE3              ) /* Supplemental SSE-3 */
        _F(X86_FEATURE_CID                ) /* Context ID */
        _F(X86_FEATURE_FMA                ) /* Fused multiply-add */
        _F(X86_FEATURE_CX16               ) /* CMPXCHG16B */
        _F(X86_FEATURE_XTPR               ) /* Send Task Priority Messages */
        _F(X86_FEATURE_PDCM               ) /* Performance Capabilities */
        _F(X86_FEATURE_PCID               ) /* Process Context Identifiers */
        _F(X86_FEATURE_DCA                ) /* Direct Cache Access */
        _F(X86_FEATURE_XMM4_1             ) /* "sse4_1" SSE-4.1 */
        _F(X86_FEATURE_XMM4_2             ) /* "sse4_2" SSE-4.2 */
        _F(X86_FEATURE_X2APIC             ) /* x2APIC */
        _F(X86_FEATURE_MOVBE              ) /* MOVBE instruction */
        _F(X86_FEATURE_POPCNT             ) /* POPCNT instruction */
        _F(X86_FEATURE_TSC_DEADLINE_TIMER ) /* Tsc deadline timer */
        _F(X86_FEATURE_AES                ) /* AES instructions */
        _F(X86_FEATURE_XSAVE              ) /* XSAVE/XRSTOR/XSETBV/XGETBV */
        _F(X86_FEATURE_OSXSAVE            ) /* "" XSAVE enabled in the OS */
        _F(X86_FEATURE_AVX                ) /* Advanced Vector Extensions */
        _F(X86_FEATURE_F16C               ) /* 16-bit fp conversions */
        _F(X86_FEATURE_RDRAND             ) /* The RDRAND instruction */
        _F(X86_FEATURE_HYPERVISOR         ) /* Running on a hypervisor */

        _F(X86_FEATURE_LAHF_LM            ) /* LAHF/SAHF in long mode */
        _F(X86_FEATURE_CMP_LEGACY         ) /* If yes HyperThreading not valid */
        _F(X86_FEATURE_SVM                ) /* Secure virtual machine */
        _F(X86_FEATURE_EXTAPIC            ) /* Extended APIC space */
        _F(X86_FEATURE_CR8_LEGACY         ) /* CR8 in 32-bit mode */
        _F(X86_FEATURE_ABM                ) /* Advanced bit manipulation */
        _F(X86_FEATURE_SSE4A              ) /* SSE-4A */
        _F(X86_FEATURE_MISALIGNSSE        ) /* Misaligned SSE mode */
        _F(X86_FEATURE_3DNOWPREFETCH      ) /* 3DNow prefetch instructions */
        _F(X86_FEATURE_OSVW               ) /* OS Visible Workaround */
        _F(X86_FEATURE_IBS                ) /* Instruction Based Sampling */
        _F(X86_FEATURE_XOP                ) /* extended AVX instructions */
        _F(X86_FEATURE_SKINIT             ) /* SKINIT/STGI instructions */
        _F(X86_FEATURE_WDT                ) /* Watchdog timer */
        _F(X86_FEATURE_LWP                ) /* Light Weight Profiling */
        _F(X86_FEATURE_FMA4               ) /* 4 operands MAC instructions */
        _F(X86_FEATURE_TCE                ) /* translation cache extension */
        _F(X86_FEATURE_NODEID_MSR         ) /* NodeId MSR */
        _F(X86_FEATURE_TBM                ) /* trailing bit manipulations */
        _F(X86_FEATURE_TOPOEXT            ) /* topology extensions CPUID leafs */
        _F(X86_FEATURE_PERFCTR_CORE       ) /* core performance counter extensions */
        _F(X86_FEATURE_PERFCTR_NB         ) /* NB performance counter extensions */
        _F(X86_FEATURE_BPEXT              ) /* data breakpoint extension */
        _F(X86_FEATURE_PERFCTR_L2         ) /* L2 performance counter extensions */

        _F(X86_FEATURE_IDA                ) /* Intel Dynamic Acceleration */
        _F(X86_FEATURE_ARAT               ) /* Always Running APIC Timer */
        _F(X86_FEATURE_CPB                ) /* AMD Core Performance Boost */
        _F(X86_FEATURE_EPB                ) /* IA32_ENERGY_PERF_BIAS support */
        _F(X86_FEATURE_PLN                ) /* Intel Power Limit Notification */
        _F(X86_FEATURE_PTS                ) /* Intel Package Thermal Status */
        _F(X86_FEATURE_DTHERM             ) /* Digital Thermal Sensor */
        _F(X86_FEATURE_HW_PSTATE          ) /* AMD HW-PState */
        _F(X86_FEATURE_PROC_FEEDBACK      ) /* AMD ProcFeedbackInterface */
        _F(X86_FEATURE_HWP                ) /* "hwp" Intel HWP */
        _F(X86_FEATURE_HWP_NOITFY         ) /* Intel HWP_NOTIFY */
        _F(X86_FEATURE_HWP_ACT_WINDOW     ) /* Intel HWP_ACT_WINDOW */
        _F(X86_FEATURE_HWP_EPP            ) /* Intel HWP_EPP */
        _F(X86_FEATURE_HWP_PKG_REQ        ) /* Intel HWP_PKG_REQ */
        _F(X86_FEATURE_INTEL_PT           ) /* Intel Processor Trace */

        _F(X86_FEATURE_FSGSBASE           ) /* {RD/WR}{FS/GS}BASE instructions*/
        _F(X86_FEATURE_TSC_ADJUST         ) /* TSC adjustment MSR 0x3b */
        _F(X86_FEATURE_BMI1               ) /* 1st group bit manipulation extensions */
        _F(X86_FEATURE_HLE                ) /* Hardware Lock Elision */
        _F(X86_FEATURE_AVX2               ) /* AVX2 instructions */
        _F(X86_FEATURE_SMEP               ) /* Supervisor Mode Execution Protection */
        _F(X86_FEATURE_BMI2               ) /* 2nd group bit manipulation extensions */
        _F(X86_FEATURE_ERMS               ) /* Enhanced REP MOVSB/STOSB */
        _F(X86_FEATURE_INVPCID            ) /* Invalidate Processor Context ID */
        _F(X86_FEATURE_RTM                ) /* Restricted Transactional Memory */
        _F(X86_FEATURE_CQM                ) /* Cache QoS Monitoring */
        _F(X86_FEATURE_MPX                ) /* Memory Protection Extension */
        _F(X86_FEATURE_AVX512F            ) /* AVX-512 Foundation */
        _F(X86_FEATURE_RDSEED             ) /* The RDSEED instruction */
        _F(X86_FEATURE_ADX                ) /* The ADCX and ADOX instructions */
        _F(X86_FEATURE_SMAP               ) /* Supervisor Mode Access Prevention */
        _F(X86_FEATURE_PCOMMIT            ) /* PCOMMIT instruction */
        _F(X86_FEATURE_CLFLUSHOPT         ) /* CLFLUSHOPT instruction */
        _F(X86_FEATURE_CLWB               ) /* CLWB instruction */
        _F(X86_FEATURE_AVX512PF           ) /* AVX-512 Prefetch */
        _F(X86_FEATURE_AVX512ER           ) /* AVX-512 Exponential and Reciprocal */
        _F(X86_FEATURE_AVX512CD           ) /* AVX-512 Conflict Detection */

        _F(X86_FEATURE_XSAVEOPT           ) /* XSAVEOPT */
        _F(X86_FEATURE_XSAVEC             ) /* XSAVEC */
        _F(X86_FEATURE_XGETBV1            ) /* XGETBV with ECX = 1 */
        _F(X86_FEATURE_XSAVES             ) /* XSAVES/XRSTORS */

        _F(X86_FEATURE_CQM_LLC            ) /* LLC QoS if 1 */
        _F(X86_FEATURE_CQM_OCCUP_LLC      ) /* LLC occupancy monitoring if 1 */



        #undef _F

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
    BUG_ON(cpu_has(index, X86_FEATURE_MSR));
    BUG_ON(cpu_has(index, X86_FEATURE_FPU));
    BUG_ON(cpu_has(index, X86_FEATURE_XMM));
    BUG_ON(cpu_has(index, X86_FEATURE_XMM2));


#if defined(__x86_64__)
    BUG_ON(cpu_has(index, X86_FEATURE_LM));
    BUG_ON(cpu_has(index, X86_FEATURE_PAE));
    BUG_ON(cpu_has(index, X86_FEATURE_GBPAGES));
    BUG_ON(cpu_has(index, X86_FEATURE_FXSR));
#endif

#if defined(CONFIG_HAVE_SMP)
    BUG_ON(cpu_has(index, X86_FEATURE_RDTSCP));
#endif




    //! Enable TSC Timer
    if(cpu_has(index, X86_FEATURE_TSC))
        x86_set_cr4(x86_get_cr4() | X86_CR4_TSD_MASK);


    //! Enable NX bit
    if(cpu_has(index, X86_FEATURE_NX))
        x86_wrmsr(X86_MSR_EFER, x86_rdmsr(X86_MSR_EFER) | X86_MSR_EFER_NXE);


    //! Enable Syscall bit
    if(cpu_has(index, X86_FEATURE_SYSCALL))
        x86_wrmsr(X86_MSR_EFER, x86_rdmsr(X86_MSR_EFER) | X86_MSR_EFER_SCE);


    //! Enable FXSAVE, FXRSTOR
    if(cpu_has(index, X86_FEATURE_FXSR))
        x86_set_cr4(x86_get_cr4() | X86_CR4_OSFXSR_MASK);


    //! Enable XSAVE and Extended states
    if(cpu_has(index, X86_FEATURE_XSAVE))
        x86_set_cr4(x86_get_cr4() | X86_CR4_OSXSAVE_MASK);


    //! Enable FSGSBASE instructions
    if(cpu_has(index, X86_FEATURE_FSGSBASE))
        x86_set_cr4(x86_get_cr4() | X86_CR4_FSGSBASE_MASK);


    //! Enable SMEP
    if(cpu_has(index, X86_FEATURE_SMEP))
        x86_set_cr4(x86_get_cr4() | X86_CR4_SMEP_MASK);


    //! Enable SMAP
    if(cpu_has(index, X86_FEATURE_SMAP))
        x86_set_cr4(x86_get_cr4() | X86_CR4_SMAP_MASK);


#if defined(CONFIG_HAVE_SMP)
    //! Write Processor ID
    if(cpu_has(index, X86_FEATURE_RDTSCP))
        x86_wrmsr(X86_MSR_TSC_AUX, index);
#endif


#if defined(__x86_64__)

    // * FIXME: Invalid Opcode
    // if(core->cpu.cores[index].xfeatures & X86_CPU_XFEATURES_FSGSBASE)
    //     x86_wrgsbase((uint64_t) &core->cpu.cores[index]);
    // else

    //x86_wrmsr(X86_MSR_KERNELGSBASE, (uint64_t) &core->cpu.cores[index]);
    x86_wrmsr(X86_MSR_GSBASE, (uint64_t) &core->cpu.cores[index]);

#endif


#if defined(__x86_64__)

    if(cpu_has(index, X86_FEATURE_SYSCALL)) {

        extern void x86_syscall_handler();

        x86_wrmsr(X86_MSR_STAR, ((uint64_t) KERNEL_CS << 32ULL) | ((uint64_t) ((USER_CS - 16) | 3) << 48ULL));
        x86_wrmsr(X86_MSR_LSTAR, (uint64_t) &x86_syscall_handler);
        x86_wrmsr(X86_MSR_FMASK, (uint64_t) 0x200ULL);

    }

#endif

    core->cpu.cores[index].flags |= SMP_CPU_FLAGS_ENABLED;

}


__percpu
uint64_t arch_cpu_get_current_id(void) {

    uint64_t id;


#if defined(CONFIG_HAVE_SMP)
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
#else
    id = 0ULL;
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
    //arch_vmm_clone(&core->cpu.cores[index].address_space, &core->bsp.address_space, ARCH_VMM_CLONE_VM | ARCH_VMM_CLONE_PRIVATE);
    memcpy(&core->cpu.cores[index].address_space, &core->bsp.address_space, sizeof(vmm_address_space_t));

    //* Map AP Startup Area
    arch_vmm_map (&core->cpu.cores[index].address_space, AP_BOOT_OFFSET, AP_BOOT_OFFSET, X86_MMU_PAGESIZE, 
        ARCH_VMM_MAP_FIXED | 
        ARCH_VMM_MAP_RDWR
    );

    // //* Map AP Stack Area
    arch_vmm_map (&core->cpu.cores[index].address_space, KERNEL_STACK_AREA + (KERNEL_STACK_SIZE * index), -1, X86_MMU_HUGE_2MB_PAGESIZE, 
        ARCH_VMM_MAP_HUGETLB  |
        ARCH_VMM_MAP_HUGE_2MB |
        ARCH_VMM_MAP_RDWR
    );




    ap_init();

    ap_get_header()->cpu   = (uint64_t) index;
    ap_get_header()->cr3   = (uint64_t) core->cpu.cores[index].address_space.pm;
    ap_get_header()->stack = (uint64_t) KERNEL_STACK_AREA + (KERNEL_STACK_SIZE * index);




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


    kprintf("x86-cpu: FAIL! starting up CPU #%d: id(%d) flags(%d) stack(%p)\n", index, core->cpu.cores[index].id, core->cpu.cores[index].flags);

}
