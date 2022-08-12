/*                                                                      
 * Author(s):                                                           
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 *                                                                      
 * This file is part of aplus.                                          
 *                                                                      
 * aplus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aplus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#ifndef _ARCH_X86_CPU_H
#define _ARCH_X86_CPU_H



#define X86_CR0_PE_MASK                 (1U << 0)
#define X86_CR0_MP_MASK                 (1U << 1)
#define X86_CR0_EM_MASK                 (1U << 2)
#define X86_CR0_TS_MASK                 (1U << 3)
#define X86_CR0_ET_MASK                 (1U << 4)
#define X86_CR0_NE_MASK                 (1U << 5)
#define X86_CR0_WP_MASK                 (1U << 16)
#define X86_CR0_AM_MASK                 (1U << 18)
#define X86_CR0_PG_MASK                 (1U << 31)



#define X86_CR4_VME_MASK                (1U << 0)
#define X86_CR4_PVI_MASK                (1U << 1)
#define X86_CR4_TSD_MASK                (1U << 2)
#define X86_CR4_DE_MASK                 (1U << 3)
#define X86_CR4_PSE_MASK                (1U << 4)
#define X86_CR4_PAE_MASK                (1U << 5)
#define X86_CR4_MCE_MASK                (1U << 6)
#define X86_CR4_PGE_MASK                (1U << 7)
#define X86_CR4_PCE_MASK                (1U << 8)
#define X86_CR4_OSFXSR_MASK             (1U << 9)
#define X86_CR4_OSXMMEXCPT_MASK         (1U << 10)
#define X86_CR4_LA57_MASK               (1U << 12)
#define X86_CR4_VMXE_MASK               (1U << 13)
#define X86_CR4_SMXE_MASK               (1U << 14)
#define X86_CR4_FSGSBASE_MASK           (1U << 16)
#define X86_CR4_PCIDE_MASK              (1U << 17)
#define X86_CR4_OSXSAVE_MASK            (1U << 18)
#define X86_CR4_SMEP_MASK               (1U << 20)
#define X86_CR4_SMAP_MASK               (1U << 21)
#define X86_CR4_PKE_MASK                (1U << 22)



#define X86_MSR_IA32_TSC                    0x10
#define X86_MSR_IA32_APICBASE               0x1B
#define X86_MSR_IA32_APICBASE_BSP           (1 << 8)
#define X86_MSR_IA32_APICBASE_ENABLE        (1 << 11)
#define X86_MSR_IA32_APICBASE_EXTD          (1 << 10)
#define X86_MSR_IA32_APICBASE_BASE          (0xFFFFFU << 12)
#define X86_MSR_IA32_FEATURE_CONTROL        0x0000003A
#define X86_MSR_TSC_ADJUST                  0x0000003B
#define X86_MSR_IA32_SPEC_CTRL              0x48
#define X86_MSR_VIRT_SSBD                   0xC001011F
#define X86_MSR_IA32_PRED_CMD               0x49
#define X86_MSR_IA32_UCODE_REV              0x8B
#define X86_MSR_IA32_CORE_CAPABILITY        0xCF
#define X86_MSR_IA32_ARCH_CAPABILITIES      0x10A
#define X86_MSR_IA32_TSX_CTRL                       0x122
#define X86_MSR_IA32_TSCDEADLINE            0x6E0
#define X86_MSR_P6_PERFCTR0                 0xC1
#define X86_MSR_IA32_SMBASE                 0x9E
#define X86_MSR_SMI_COUNT                   0x34
#define X86_MSR_MTRRcap                     0xFE
#define X86_MSR_MTRRcap_VCNT                8
#define X86_MSR_MTRRcap_FIXRANGE_SUPPORT    (1 << 8)
#define X86_MSR_MTRRcap_WC_SUPPORTED        (1 << 10)

#define X86_MSR_IA32_SYSENTER_CS            0x174
#define X86_MSR_IA32_SYSENTER_ESP           0x175
#define X86_MSR_IA32_SYSENTER_EIP           0x176

#define X86_MSR_MCG_CAP                     0x179
#define X86_MSR_MCG_STATUS                  0x17A
#define X86_MSR_MCG_CTL                     0x17B
#define X86_MSR_MCG_EXT_CTL                 0x4D0

#define X86_MSR_P6_EVNTSEL0                 0x186
#define X86_MSR_IA32_PERF_STATUS            0x198
#define X86_MSR_IA32_MISC_ENABLE            0x1A0
#define X86_MSR_IA32_MISC_ENABLE_DEFAULT    1
#define X86_MSR_IA32_MISC_ENABLE_MWAIT      (1ULL << 18)


#define X86_MSR_PAT                         0x277

#define X86_MSR_CORE_PERF_FIXED_CTR0        0x309
#define X86_MSR_CORE_PERF_FIXED_CTR1        0x30A
#define X86_MSR_CORE_PERF_FIXED_CTR2        0x30B
#define X86_MSR_CORE_PERF_FIXED_CTR_CTRL    0x38D
#define X86_MSR_CORE_PERF_GLOBAL_STATUS     0x38E
#define X86_MSR_CORE_PERF_GLOBAL_CTRL       0x38F
#define X86_MSR_CORE_PERF_GLOBAL_OVF_CTRL   0x390

#define X86_MSR_MC0_CTL                     0x400
#define X86_MSR_MC0_STATUS                  0x401
#define X86_MSR_MC0_ADDR                    0x402
#define X86_MSR_MC0_MISC                    0x403

#define X86_MSR_IA32_RTIT_OUTPUT_BASE       0x560
#define X86_MSR_IA32_RTIT_OUTPUT_MASK       0x561
#define X86_MSR_IA32_RTIT_CTL               0x570
#define X86_MSR_IA32_RTIT_STATUS            0x571
#define X86_MSR_IA32_RTIT_CR3_MATCH         0x572
#define X86_MSR_IA32_RTIT_ADDR0_A           0x580
#define X86_MSR_IA32_RTIT_ADDR0_B           0x581
#define X86_MSR_IA32_RTIT_ADDR1_A           0x582
#define X86_MSR_IA32_RTIT_ADDR1_B           0x583
#define X86_MSR_IA32_RTIT_ADDR2_A           0x584
#define X86_MSR_IA32_RTIT_ADDR2_B           0x585
#define X86_MSR_IA32_RTIT_ADDR3_A           0x586
#define X86_MSR_IA32_RTIT_ADDR3_B           0x587



#define X86_MSR_EFER                        0xC0000080

#define X86_MSR_EFER_SCE                    (1 << 0)
#define X86_MSR_EFER_LME                    (1 << 8)
#define X86_MSR_EFER_LMA                    (1 << 10)
#define X86_MSR_EFER_NXE                    (1 << 11)
#define X86_MSR_EFER_SVME                   (1 << 12)
#define X86_MSR_EFER_LMSLE                  (1 << 13)
#define X86_MSR_EFER_FFXSR                  (1 << 14)
#define X86_MSR_EFER_TCE                    (1 << 15)


#define X86_MSR_STAR                        0xC0000081
#define X86_MSR_LSTAR                       0xC0000082
#define X86_MSR_CSTAR                       0xC0000083
#define X86_MSR_FMASK                       0xC0000084
#define X86_MSR_FSBASE                      0xC0000100
#define X86_MSR_GSBASE                      0xC0000101
#define X86_MSR_KERNELGSBASE                0xC0000102
#define X86_MSR_TSC_AUX                     0xC0000103

#define X86_MSR_VM_HSAVE_PA                 0xC0010117

#define X86_MSR_IA32_BNDCFGS                0x00000D90
#define X86_MSR_IA32_XSS                    0x00000DA0
#define X86_MSR_IA32_UMWAIT_CONTROL         0xE1



#define XSTATE_FP_MASK                      (1ULL << 0)
#define XSTATE_SSE_MASK                     (1ULL << 1)
#define XSTATE_YMM_MASK                     (1ULL << 2)
#define XSTATE_BNDREGS_MASK                 (1ULL << 3)
#define XSTATE_BNDCSR_MASK                  (1ULL << 4)
#define XSTATE_OPMASK_MASK                  (1ULL << 5)
#define XSTATE_ZMM_Hi256_MASK               (1ULL << 6)
#define XSTATE_Hi16_ZMM_MASK                (1ULL << 7)
#define XSTATE_PKRU_MASK                    (1ULL << 9)




#ifndef __ASSEMBLY__

/*
 * @see asm/processor.h (Linux)
 * Note: If the comment begins with a quoted string, that string is used
 * in /proc/cpuinfo instead of the macro name.  If the string is "",
 * this feature bit is not displayed in /proc/cpuinfo at all.
 */

/* Intel-defined CPU features, CPUID level 0x00000001 (edx), word 0 */
#define X86_FEATURE_FPU                     ( 0*32+ 0) /* Onboard FPU */
#define X86_FEATURE_VME                     ( 0*32+ 1) /* Virtual Mode Extensions */
#define X86_FEATURE_DE                      ( 0*32+ 2) /* Debugging Extensions */
#define X86_FEATURE_PSE                     ( 0*32+ 3) /* Page Size Extensions */
#define X86_FEATURE_TSC                     ( 0*32+ 4) /* Time Stamp Counter */
#define X86_FEATURE_MSR                     ( 0*32+ 5) /* Model-Specific Registers */
#define X86_FEATURE_PAE                     ( 0*32+ 6) /* Physical Address Extensions */
#define X86_FEATURE_MCE                     ( 0*32+ 7) /* Machine Check Exception */
#define X86_FEATURE_CX8                     ( 0*32+ 8) /* CMPXCHG8 instruction */
#define X86_FEATURE_APIC                    ( 0*32+ 9) /* Onboard APIC */
#define X86_FEATURE_SEP                     ( 0*32+11) /* SYSENTER/SYSEXIT */
#define X86_FEATURE_MTRR                    ( 0*32+12) /* Memory Type Range Registers */
#define X86_FEATURE_PGE                     ( 0*32+13) /* Page Global Enable */
#define X86_FEATURE_MCA                     ( 0*32+14) /* Machine Check Architecture */
#define X86_FEATURE_CMOV                    ( 0*32+15) /* CMOV instructions */
                                                       /* (plus FCMOVcc, FCOMI with FPU) */
#define X86_FEATURE_PAT                     ( 0*32+16) /* Page Attribute Table */
#define X86_FEATURE_PSE36                   ( 0*32+17) /* 36-bit PSEs */
#define X86_FEATURE_PN                      ( 0*32+18) /* Processor serial number */
#define X86_FEATURE_CLFLUSH                 ( 0*32+19) /* CLFLUSH instruction */
#define X86_FEATURE_DS                      ( 0*32+21) /* "dts" Debug Store */
#define X86_FEATURE_ACPI                    ( 0*32+22) /* ACPI via MSR */
#define X86_FEATURE_MMX                     ( 0*32+23) /* Multimedia Extensions */
#define X86_FEATURE_FXSR                    ( 0*32+24) /* FXSAVE/FXRSTOR, CR4.OSFXSR */
#define X86_FEATURE_XMM                     ( 0*32+25) /* "sse" */
#define X86_FEATURE_XMM2                    ( 0*32+26) /* "sse2" */
#define X86_FEATURE_SELFSNOOP               ( 0*32+27) /* "ss" CPU self snoop */
#define X86_FEATURE_HT                      ( 0*32+28) /* Hyper-Threading */
#define X86_FEATURE_ACC                     ( 0*32+29) /* "tm" Automatic clock control */
#define X86_FEATURE_IA64                    ( 0*32+30) /* IA-64 processor */
#define X86_FEATURE_PBE                     ( 0*32+31) /* Pending Break Enable */


/* AMD-defined CPU features, CPUID level 0x80000001, word 1 */
/* Don't duplicate feature flags which are redundant with Intel! */
#define X86_FEATURE_SYSCALL                 ( 1*32+11) /* SYSCALL/SYSRET */
#define X86_FEATURE_MP                      ( 1*32+19) /* MP Capable. */
#define X86_FEATURE_NX                      ( 1*32+20) /* Execute Disable */
#define X86_FEATURE_MMXEXT                  ( 1*32+22) /* AMD MMX extensions */
#define X86_FEATURE_FXSR_OPT                ( 1*32+25) /* FXSAVE/FXRSTOR optimizations */
#define X86_FEATURE_GBPAGES                 ( 1*32+26) /* "pdpe1gb" GB pages */
#define X86_FEATURE_RDTSCP                  ( 1*32+27) /* RDTSCP */
#define X86_FEATURE_LM                      ( 1*32+29) /* Long Mode (x86-64) */
#define X86_FEATURE_3DNOWEXT                ( 1*32+30) /* AMD 3DNow! extensions */
#define X86_FEATURE_3DNOW                   ( 1*32+31) /* 3DNow! */


/* Transmeta-defined CPU features, CPUID level 0x80860001, word 2 */
#define X86_FEATURE_RECOVERY                ( 2*32+ 0) /* CPU in recovery mode */
#define X86_FEATURE_LONGRUN                 ( 2*32+ 1) /* Longrun power control */
#define X86_FEATURE_LRTI                    ( 2*32+ 3) /* LongRun table interface */


// TODO: Implements features
/* Other features, Linux-defined mapping, word 3 */
/* This range is used for feature bits which conflict or are synthesized */
#define X86_FEATURE_CONSTANT_TSC            ( 3*32+ 8) /* TSC ticks at a constant rate */
#define X86_FEATURE_ARCH_PERFMON            ( 3*32+11) /* Intel Architectural PerfMon */
#define X86_FEATURE_PEBS                    ( 3*32+12) /* Precise-Event Based Sampling */
#define X86_FEATURE_BTS                     ( 3*32+13) /* Branch Trace Store */
#define X86_FEATURE_MFENCE_RDTSC            ( 3*32+17) /* "" Mfence synchronizes RDTSC */
#define X86_FEATURE_LFENCE_RDTSC            ( 3*32+18) /* "" Lfence synchronizes RDTSC */
#define X86_FEATURE_NONSTOP_TSC             ( 3*32+24) /* TSC does not stop in C states */
#define X86_FEATURE_APERFMPERF              ( 3*32+28) /* APERFMPERF */
#define X86_FEATURE_NONSTOP_TSC_S3          ( 3*32+30) /* TSC doesn't stop in S3 state */


/* Intel-defined CPU features, CPUID level 0x00000001 (ecx), word 4 */
#define X86_FEATURE_XMM3                    ( 4*32+ 0) /* "pni" SSE-3 */
#define X86_FEATURE_PCLMULQDQ               ( 4*32+ 1) /* PCLMULQDQ instruction */
#define X86_FEATURE_DTES64                  ( 4*32+ 2) /* 64-bit Debug Store */
#define X86_FEATURE_MWAIT                   ( 4*32+ 3) /* "monitor" Monitor/Mwait support */
#define X86_FEATURE_DSCPL                   ( 4*32+ 4) /* "ds_cpl" CPL Qual. Debug Store */
#define X86_FEATURE_VMX                     ( 4*32+ 5) /* Hardware virtualization */
#define X86_FEATURE_SMX                     ( 4*32+ 6) /* Safer mode */
#define X86_FEATURE_EST                     ( 4*32+ 7) /* Enhanced SpeedStep */
#define X86_FEATURE_TM2                     ( 4*32+ 8) /* Thermal Monitor 2 */
#define X86_FEATURE_SSSE3                   ( 4*32+ 9) /* Supplemental SSE-3 */
#define X86_FEATURE_CID                     ( 4*32+10) /* Context ID */
#define X86_FEATURE_FMA                     ( 4*32+12) /* Fused multiply-add */
#define X86_FEATURE_CX16                    ( 4*32+13) /* CMPXCHG16B */
#define X86_FEATURE_XTPR                    ( 4*32+14) /* Send Task Priority Messages */
#define X86_FEATURE_PDCM                    ( 4*32+15) /* Performance Capabilities */
#define X86_FEATURE_PCID                    ( 4*32+17) /* Process Context Identifiers */
#define X86_FEATURE_DCA                     ( 4*32+18) /* Direct Cache Access */
#define X86_FEATURE_XMM4_1                  ( 4*32+19) /* "sse4_1" SSE-4.1 */
#define X86_FEATURE_XMM4_2                  ( 4*32+20) /* "sse4_2" SSE-4.2 */
#define X86_FEATURE_X2APIC                  ( 4*32+21) /* x2APIC */
#define X86_FEATURE_MOVBE                   ( 4*32+22) /* MOVBE instruction */
#define X86_FEATURE_POPCNT                  ( 4*32+23) /* POPCNT instruction */
#define X86_FEATURE_TSC_DEADLINE_TIMER      ( 4*32+24) /* Tsc deadline timer */
#define X86_FEATURE_AES                     ( 4*32+25) /* AES instructions */
#define X86_FEATURE_XSAVE                   ( 4*32+26) /* XSAVE/XRSTOR/XSETBV/XGETBV */
#define X86_FEATURE_OSXSAVE                 ( 4*32+27) /* "" XSAVE enabled in the OS */
#define X86_FEATURE_AVX                     ( 4*32+28) /* Advanced Vector Extensions */
#define X86_FEATURE_F16C                    ( 4*32+29) /* 16-bit fp conversions */
#define X86_FEATURE_RDRAND                  ( 4*32+30) /* The RDRAND instruction */
#define X86_FEATURE_HYPERVISOR              ( 4*32+31) /* Running on a hypervisor */



/* More extended AMD flags: CPUID level 0x80000001, ecx, word 6 */
#define X86_FEATURE_LAHF_LM                 ( 6*32+ 0) /* LAHF/SAHF in long mode */
#define X86_FEATURE_CMP_LEGACY              ( 6*32+ 1) /* If yes HyperThreading not valid */
#define X86_FEATURE_SVM                     ( 6*32+ 2) /* Secure virtual machine */
#define X86_FEATURE_EXTAPIC                 ( 6*32+ 3) /* Extended APIC space */
#define X86_FEATURE_CR8_LEGACY              ( 6*32+ 4) /* CR8 in 32-bit mode */
#define X86_FEATURE_ABM                     ( 6*32+ 5) /* Advanced bit manipulation */
#define X86_FEATURE_SSE4A                   ( 6*32+ 6) /* SSE-4A */
#define X86_FEATURE_MISALIGNSSE             ( 6*32+ 7) /* Misaligned SSE mode */
#define X86_FEATURE_3DNOWPREFETCH           ( 6*32+ 8) /* 3DNow prefetch instructions */
#define X86_FEATURE_OSVW                    ( 6*32+ 9) /* OS Visible Workaround */
#define X86_FEATURE_IBS                     ( 6*32+10) /* Instruction Based Sampling */
#define X86_FEATURE_XOP                     ( 6*32+11) /* extended AVX instructions */
#define X86_FEATURE_SKINIT                  ( 6*32+12) /* SKINIT/STGI instructions */
#define X86_FEATURE_WDT                     ( 6*32+13) /* Watchdog timer */
#define X86_FEATURE_LWP                     ( 6*32+15) /* Light Weight Profiling */
#define X86_FEATURE_FMA4                    ( 6*32+16) /* 4 operands MAC instructions */
#define X86_FEATURE_TCE                     ( 6*32+17) /* translation cache extension */
#define X86_FEATURE_NODEID_MSR              ( 6*32+19) /* NodeId MSR */
#define X86_FEATURE_TBM                     ( 6*32+21) /* trailing bit manipulations */
#define X86_FEATURE_TOPOEXT                 ( 6*32+22) /* topology extensions CPUID leafs */
#define X86_FEATURE_PERFCTR_CORE            ( 6*32+23) /* core performance counter extensions */
#define X86_FEATURE_PERFCTR_NB              ( 6*32+24) /* NB performance counter extensions */
#define X86_FEATURE_BPEXT                   ( 6*32+26) /* data breakpoint extension */
#define X86_FEATURE_PERFCTR_L2              ( 6*32+28) /* L2 performance counter extensions */


// TODO: Implements features
/*
 * Auxiliary flags: Linux defined - For features scattered in various
 * CPUID levels like 0x6, 0xA etc, word 7
 */
#define X86_FEATURE_IDA                     ( 7*32+ 0) /* Intel Dynamic Acceleration */
#define X86_FEATURE_ARAT                    ( 7*32+ 1) /* Always Running APIC Timer */
#define X86_FEATURE_CPB                     ( 7*32+ 2) /* AMD Core Performance Boost */
#define X86_FEATURE_EPB                     ( 7*32+ 3) /* IA32_ENERGY_PERF_BIAS support */
#define X86_FEATURE_PLN                     ( 7*32+ 5) /* Intel Power Limit Notification */
#define X86_FEATURE_PTS                     ( 7*32+ 6) /* Intel Package Thermal Status */
#define X86_FEATURE_DTHERM                  ( 7*32+ 7) /* Digital Thermal Sensor */
#define X86_FEATURE_HW_PSTATE               ( 7*32+ 8) /* AMD HW-PState */
#define X86_FEATURE_PROC_FEEDBACK           ( 7*32+ 9) /* AMD ProcFeedbackInterface */
#define X86_FEATURE_HWP                     ( 7*32+ 10) /* "hwp" Intel HWP */
#define X86_FEATURE_HWP_NOITFY              ( 7*32+ 11) /* Intel HWP_NOTIFY */
#define X86_FEATURE_HWP_ACT_WINDOW          ( 7*32+ 12) /* Intel HWP_ACT_WINDOW */
#define X86_FEATURE_HWP_EPP                 ( 7*32+13) /* Intel HWP_EPP */
#define X86_FEATURE_HWP_PKG_REQ             ( 7*32+14) /* Intel HWP_PKG_REQ */
#define X86_FEATURE_INTEL_PT                ( 7*32+15) /* Intel Processor Trace */



/* Intel-defined CPU features, CPUID level 0x00000007:0 (ebx), word 9 */
#define X86_FEATURE_FSGSBASE                ( 9*32+ 0) /* {RD/WR}{FS/GS}BASE instructions*/
#define X86_FEATURE_TSC_ADJUST              ( 9*32+ 1) /* TSC adjustment MSR 0x3b */
#define X86_FEATURE_BMI1                    ( 9*32+ 3) /* 1st group bit manipulation extensions */
#define X86_FEATURE_HLE                     ( 9*32+ 4) /* Hardware Lock Elision */
#define X86_FEATURE_AVX2                    ( 9*32+ 5) /* AVX2 instructions */
#define X86_FEATURE_SMEP                    ( 9*32+ 7) /* Supervisor Mode Execution Protection */
#define X86_FEATURE_BMI2                    ( 9*32+ 8) /* 2nd group bit manipulation extensions */
#define X86_FEATURE_ERMS                    ( 9*32+ 9) /* Enhanced REP MOVSB/STOSB */
#define X86_FEATURE_INVPCID                 ( 9*32+10) /* Invalidate Processor Context ID */
#define X86_FEATURE_RTM                     ( 9*32+11) /* Restricted Transactional Memory */
#define X86_FEATURE_CQM                     ( 9*32+12) /* Cache QoS Monitoring */
#define X86_FEATURE_MPX                     ( 9*32+14) /* Memory Protection Extension */
#define X86_FEATURE_AVX512F                 ( 9*32+16) /* AVX-512 Foundation */
#define X86_FEATURE_RDSEED                  ( 9*32+18) /* The RDSEED instruction */
#define X86_FEATURE_ADX                     ( 9*32+19) /* The ADCX and ADOX instructions */
#define X86_FEATURE_SMAP                    ( 9*32+20) /* Supervisor Mode Access Prevention */
#define X86_FEATURE_PCOMMIT                 ( 9*32+22) /* PCOMMIT instruction */
#define X86_FEATURE_CLFLUSHOPT              ( 9*32+23) /* CLFLUSHOPT instruction */
#define X86_FEATURE_CLWB                    ( 9*32+24) /* CLWB instruction */
#define X86_FEATURE_AVX512PF                ( 9*32+26) /* AVX-512 Prefetch */
#define X86_FEATURE_AVX512ER                ( 9*32+27) /* AVX-512 Exponential and Reciprocal */
#define X86_FEATURE_AVX512CD                ( 9*32+28) /* AVX-512 Conflict Detection */


/* Extended state features, CPUID level 0x0000000d:1 (eax), word 10 */
#define X86_FEATURE_XSAVEOPT                (10*32+ 0) /* XSAVEOPT */
#define X86_FEATURE_XSAVEC                  (10*32+ 1) /* XSAVEC */
#define X86_FEATURE_XGETBV1                 (10*32+ 2) /* XGETBV with ECX = 1 */
#define X86_FEATURE_XSAVES                  (10*32+ 3) /* XSAVES/XRSTORS */


/* Intel-defined CPU QoS Sub-leaf, CPUID level 0x0000000F:0 (edx), word 11 */
#define X86_FEATURE_CQM_LLC                 (11*32+ 1) /* LLC QoS if 1 */

/* Intel-defined CPU QoS Sub-leaf, CPUID level 0x0000000F:1 (edx), word 12 */
#define X86_FEATURE_CQM_OCCUP_LLC           (12*32+ 0) /* LLC occupancy monitoring if 1 */





typedef struct tss {

    union {
        uint32_t __rsv0;
        uint16_t link;
    };

    uintptr_t sp0;

#if defined(__x86_64__)
    uintptr_t sp1;
    uintptr_t sp2;
    uintptr_t __rsv1;
    uintptr_t ist[7];
    uintptr_t __rsv2;
#else
    uintptr_t ss0;
    uintptr_t sp1;
    uintptr_t ss1;
    uintptr_t sp2;
    uintptr_t ss2;
    uintptr_t cr3;
    uintptr_t ip;
    uintptr_t flags;
    uintptr_t ax;
    uintptr_t cx;
    uintptr_t dx;
    uintptr_t bx;
    uintptr_t sp;
    uintptr_t bp;
    uintptr_t si;
    uintptr_t di;
    uintptr_t es;
    uintptr_t cs;
    uintptr_t ss;
    uintptr_t ds;
    uintptr_t fs;
    uintptr_t gs;
    uintptr_t ldtr;
#endif

    uint32_t iopb;


    uint64_t __padding[3];

} __packed tss_t;



/*!
 * @brief Get x86 Register value.
 */
#define x86_get_reg(p, e)                                                       \
    static inline long x86_get_##e() {                                          \
        long r;                                                                 \
        __asm__ __volatile__ ("mov" #p " %%" #e ", %0" : "=r"(r));              \
        return r;                                                               \
    }


    x86_get_reg(l, eax)
    x86_get_reg(l, ebx)
    x86_get_reg(l, ecx)
    x86_get_reg(l, edx)
    x86_get_reg(l, esi)
    x86_get_reg(l, edi)
    x86_get_reg(l, ebp)
    x86_get_reg(l, esp)

#if defined(__x86_64__)

    x86_get_reg(q, rax)
    x86_get_reg(q, rbx)
    x86_get_reg(q, rcx)
    x86_get_reg(q, rdx)
    x86_get_reg(q, rsi)
    x86_get_reg(q, rdi)
    x86_get_reg(q, rbp)
    x86_get_reg(q, rsp)

    x86_get_reg(q, cr0)
    x86_get_reg(q, cr1)
    x86_get_reg(q, cr2)
    x86_get_reg(q, cr3)
    x86_get_reg(q, cr4)
    
    x86_get_reg(q, cs)
    x86_get_reg(q, ds)
    x86_get_reg(q, es)
    x86_get_reg(q, fs)
    x86_get_reg(q, gs)

    x86_get_reg(q, r8)
    x86_get_reg(q, r9)
    x86_get_reg(q, r10)
    x86_get_reg(q, r11)
    x86_get_reg(q, r12)


    static inline long x86_get_flags() {
        long r;
        __asm__ __volatile__ (
            "pushfq; popq %%rax"
            : "=a"(r)
        );
        return r;
    }

#elif defined(__i386__)
#error "i386: not supported"
#endif



/*!
 * @brief Set x86 Register value.
 */
#define x86_set_reg(p, e)                                                       \
    static inline void x86_set_##e(long r) {                                    \
        __asm__ __volatile__ ("mov" #p " %0, %%" #e :: "r"(r));                 \
    }


    x86_set_reg(l, eax)
    x86_set_reg(l, ebx)
    x86_set_reg(l, ecx)
    x86_set_reg(l, edx)
    x86_set_reg(l, esi)
    x86_set_reg(l, edi)
    x86_set_reg(l, ebp)
    x86_set_reg(l, esp)

#if defined(__x86_64__)

    x86_set_reg(q, rax)
    x86_set_reg(q, rbx)
    x86_set_reg(q, rcx)
    x86_set_reg(q, rdx)
    x86_set_reg(q, rsi)
    x86_set_reg(q, rdi)
    x86_set_reg(q, rbp)
    x86_set_reg(q, rsp)

    x86_set_reg(q, cr0)
    x86_set_reg(q, cr1)
    x86_set_reg(q, cr2)
    x86_set_reg(q, cr3)
    x86_set_reg(q, cr4)
    
    x86_set_reg(q, cs)
    x86_set_reg(q, ds)
    x86_set_reg(q, es)
    x86_set_reg(q, fs)
    x86_set_reg(q, gs)

    x86_set_reg(q, r8)
    x86_set_reg(q, r9)
    x86_set_reg(q, r10)
    x86_set_reg(q, r11)
    x86_set_reg(q, r12)


    static inline void x86_set_flags(long r) {
        __asm__ __volatile__ (
            "pushq %rax; popfq"
            :: "a"(r)
        );
    }

#elif defined(__i386__)
#error "i386: not supported"
#endif



/*!
 * @brief Output Data to I/O Port.
 */
#define outx(n, t, reg)                                                             \
    static inline void out##n(unsigned short p, t v) {                              \
        __asm__ __volatile__ ("out" #n " %%" #reg ", %%dx" : : "a"(v), "d"(p));     \
    }
    
    outx(b, unsigned char, al)
    outx(w, unsigned short, ax)
    outx(l, unsigned int, eax)
    
    
/*!
 * @brief Input Data from I/O Port.
 */
#define inx(n, t, reg)                                                      \
    static inline t in##n(unsigned short p) {                               \
        t r;                                                                \
        __asm__ __volatile__ ("in" #n " %%dx, %%" #reg : "=a"(r) : "d"(p)); \
        return r;                                                           \
    }
    
    inx(b, unsigned char, al)
    inx(w, unsigned short, ax)
    inx(l, unsigned int, eax)
    

/*!
 * @brief Output Buffer to I/O Port.
 */
#define outsx(n, t, reg)                                                        \
    static inline void outs##n(unsigned short p, t* v, unsigned int len) {      \
        for(int i = 0; i < len; i++)                                            \
            out##n(p, v[i]);                                                    \
    }
    
    outsx(b, unsigned char, al)
    outsx(w, unsigned short, ax)
    outsx(l, unsigned int, eax)
    
    

/*!
 * @brief Input Buffer from I/O Port.
 */
#define insx(n, t, reg)                                                         \
    static inline t* ins##n(unsigned short p, t* v, unsigned int len) {         \
        for(int i = 0; i < len; i++)                                            \
            v[i] = in##n(p);                                                    \
                                                                                \
        return v;                                                               \
    }
    
    insx(b, unsigned char, al)
    insx(w, unsigned short, ax)
    insx(l, unsigned int, eax)



/*!
 * @brief Write to Model Specific Register.
 */
static inline void x86_wrmsr(unsigned long long i, unsigned long long v) {

    unsigned long long vl = v & 0xFFFFFFFF;
    unsigned long long vh = v >> 32;

    __asm__ __volatile__ (
        "wrmsr" 
        : 
        : "c"(i), "a"(vl), "d"(vh)
    );
}


/*!
 * @brief Read from Model Specific Register.
 */
static inline unsigned long long x86_rdmsr(unsigned long long i) {

    unsigned long long vl;
    unsigned long long vh;

    __asm__ __volatile__ (
        "rdmsr" 
        : "=a"(vl), "=d"(vh) 
        : "c"(i)
    );

    return (vh << 32) | vl;
}


/*!
 * @brief Read Time-Stamp Counter.
 */
static inline unsigned long long x86_rdtsc(void) {

    unsigned long long vl;
    unsigned long long vh;

    __asm__ __volatile__ (
        "lfence; rdtsc; lfence;" 
        : "=a"(vl), "=d"(vh)
    );

    return (vh << 32) | vl;
}


/*!
 * @brief CPU Identification.
 * 
 * Returns processor identification and feature information to the EAX, EBX, ECX,
 * and EDX registers, as determined by input entered in EAX (in some cases, ECX as well).
 */
static inline void x86_cpuid(long r, long* a, long* b, long* c, long* d) {
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) 
        :  "a"(r), "c"(0)
    );
}


/*!
 * @brief CPU Identification.
 * 
 * Returns processor identification and feature information to the EAX, EBX, ECX,
 * and EDX registers, as determined by input entered in EAX (in some cases, ECX as well).
 */
static inline void x86_cpuid_extended(long r, long* a, long* b, long* c, long* d) {
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) 
        :  "a"(r),   "b"(*b),  "c"(*c),  "d"(*d)
    );
}



#if defined(__x86_64__)

/*!
 * @brief Write GS Base Register.
 */
static inline void x86_wrgsbase(unsigned long long base) {
    __asm__ __volatile__ (
        "wrgsbase %0"
        :
        : "r"(base)
        : "memory"
    );
}

/*!
 * @brief Write FS Base Register.
 */
static inline void x86_wrfsbase(unsigned long long base) {
    __asm__ __volatile__ (
        "wrfsbase %0"
        :
        : "r"(base)
        : "memory"
    );
}

/*!
 * @brief Read GS Base Register.
 */
static inline unsigned long long x86_rdgsbase() {
    unsigned long long r = 0;
    __asm__ __volatile__ (
        "rdgsbase %0"
        : "=r"(r)
        :
        : "memory"
    );
    
    return r;
}

/*!
 * @brief Read FS Base Register.
 */
static inline unsigned long long x86_rdfsbase() {
    unsigned long long r = 0;
    __asm__ __volatile__ (
        "rdfsbase %0"
        : "=r"(r)
        :
        : "memory"
    );

    return r;
}

/*!
 * @brief Swap GS/KernelGS Base Register.
 */
static inline void x86_swapgs() {
    __asm__ __volatile__ (
        "swapgs"
    );
}


/*!
 * @brief Write to Extended Control Register.
 */
static inline void x86_xsetbv(unsigned long long i, unsigned long long v) {

    unsigned long long vl = v & 0xFFFFFFFF;
    unsigned long long vh = v >> 32;

    __asm__ __volatile__ (
        "xsetbv" 
        : 
        : "c"(i), "a"(vl), "d"(vh)
    );
}


/*!
 * @brief Read from Extended Control Register.
 */
static inline unsigned long long x86_xgetbv(unsigned long long i) {

    unsigned long long vl;
    unsigned long long vh;

    __asm__ __volatile__ (
        "xgetbv" 
        : "=a"(vl), "=d"(vh) 
        : "c"(i)
    );

    return (vh << 32) | vl;
}


#endif


__BEGIN_DECLS

// @see arch/x86-family/random.c
void random_init(void);

__END_DECLS

#endif
#endif
