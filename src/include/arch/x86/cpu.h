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
#define X86_MSR_IA32_TSX_CTRL		        0x122
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



/* CPUID feature words */
typedef enum FeatureWord {

    FEAT_1_EDX,         /* CPUID[1].EDX */
    FEAT_1_ECX,         /* CPUID[1].ECX */
    FEAT_7_0_EBX,       /* CPUID[EAX=7,ECX=0].EBX */
    FEAT_7_0_ECX,       /* CPUID[EAX=7,ECX=0].ECX */
    FEAT_7_0_EDX,       /* CPUID[EAX=7,ECX=0].EDX */
    FEAT_7_1_EAX,       /* CPUID[EAX=7,ECX=1].EAX */
    FEAT_8000_0001_EDX, /* CPUID[8000_0001].EDX */
    FEAT_8000_0001_ECX, /* CPUID[8000_0001].ECX */
    FEAT_8000_0007_EDX, /* CPUID[8000_0007].EDX */
    FEAT_8000_0008_EBX, /* CPUID[8000_0008].EBX */
    FEAT_C000_0001_EDX, /* CPUID[C000_0001].EDX */
    FEAT_KVM,           /* CPUID[4000_0001].EAX (KVM_CPUID_FEATURES) */
    FEAT_KVM_HINTS,     /* CPUID[4000_0001].EDX */
    FEAT_HYPERV_EAX,    /* CPUID[4000_0003].EAX */
    FEAT_HYPERV_EBX,    /* CPUID[4000_0003].EBX */
    FEAT_HYPERV_EDX,    /* CPUID[4000_0003].EDX */
    FEAT_HV_RECOMM_EAX, /* CPUID[4000_0004].EAX */
    FEAT_HV_NESTED_EAX, /* CPUID[4000_000A].EAX */
    FEAT_SVM,           /* CPUID[8000_000A].EDX */
    FEAT_XSAVE,         /* CPUID[EAX=0xd,ECX=1].EAX */
    FEAT_6_EAX,         /* CPUID[6].EAX */
    FEAT_XSAVE_COMP_LO, /* CPUID[EAX=0xd,ECX=0].EAX */
    FEAT_XSAVE_COMP_HI, /* CPUID[EAX=0xd,ECX=0].EDX */
    FEAT_ARCH_CAPABILITIES,
    FEAT_CORE_CAPABILITY,
    FEAT_VMX_PROCBASED_CTLS,
    FEAT_VMX_SECONDARY_CTLS,
    FEAT_VMX_PINBASED_CTLS,
    FEAT_VMX_EXIT_CTLS,
    FEAT_VMX_ENTRY_CTLS,
    FEAT_VMX_MISC,
    FEAT_VMX_EPT_VPID_CAPS,
    FEAT_VMX_BASIC,
    FEAT_VMX_VMFUNC,
    FEATURE_WORDS,

} FeatureWord;




/* FEAT_1_EDX */
#define X86_CPUID_FP87              (1U << 0)
#define X86_CPUID_VME               (1U << 1)
#define X86_CPUID_DE                (1U << 2)
#define X86_CPUID_PSE               (1U << 3)
#define X86_CPUID_TSC               (1U << 4)
#define X86_CPUID_MSR               (1U << 5)
#define X86_CPUID_PAE               (1U << 6)
#define X86_CPUID_MCE               (1U << 7)
#define X86_CPUID_CX8               (1U << 8)
#define X86_CPUID_APIC              (1U << 9)
#define X86_CPUID_SEP               (1U << 11) /* sysenter/sysexit */
#define X86_CPUID_MTRR              (1U << 12)
#define X86_CPUID_PGE               (1U << 13)
#define X86_CPUID_MCA               (1U << 14)
#define X86_CPUID_CMOV              (1U << 15)
#define X86_CPUID_PAT               (1U << 16)
#define X86_CPUID_PSE36             (1U << 17)
#define X86_CPUID_PN                (1U << 18)
#define X86_CPUID_CLFLUSH           (1U << 19)
#define X86_CPUID_DTS               (1U << 21)
#define X86_CPUID_ACPI              (1U << 22)
#define X86_CPUID_MMX               (1U << 23)
#define X86_CPUID_FXSR              (1U << 24)
#define X86_CPUID_SSE               (1U << 25)
#define X86_CPUID_SSE2              (1U << 26)
#define X86_CPUID_SS                (1U << 27)
#define X86_CPUID_HT                (1U << 28)
#define X86_CPUID_TM                (1U << 29)
#define X86_CPUID_IA64              (1U << 30)
#define X86_CPUID_PBE               (1U << 31)

/* FEAT_1_ECX */
#define X86_CPUID_EXT_SSE3          (1U << 0)
#define X86_CPUID_EXT_PCLMULQDQ     (1U << 1)
#define X86_CPUID_EXT_DTES64        (1U << 2)
#define X86_CPUID_EXT_MONITOR       (1U << 3)
#define X86_CPUID_EXT_DSCPL         (1U << 4)
#define X86_CPUID_EXT_VMX           (1U << 5)
#define X86_CPUID_EXT_SMX           (1U << 6)
#define X86_CPUID_EXT_EST           (1U << 7)
#define X86_CPUID_EXT_TM2           (1U << 8)
#define X86_CPUID_EXT_SSSE3         (1U << 9)
#define X86_CPUID_EXT_CID           (1U << 10)
#define X86_CPUID_EXT_FMA           (1U << 12)
#define X86_CPUID_EXT_CX16          (1U << 13)
#define X86_CPUID_EXT_XTPR          (1U << 14)
#define X86_CPUID_EXT_PDCM          (1U << 15)
#define X86_CPUID_EXT_PCID          (1U << 17)
#define X86_CPUID_EXT_DCA           (1U << 18)
#define X86_CPUID_EXT_SSE41         (1U << 19)
#define X86_CPUID_EXT_SSE42         (1U << 20)
#define X86_CPUID_EXT_X2APIC        (1U << 21)
#define X86_CPUID_EXT_MOVBE         (1U << 22)
#define X86_CPUID_EXT_POPCNT        (1U << 23)
#define X86_CPUID_EXT_TSC_DEADLINE_TIMER (1U << 24)
#define X86_CPUID_EXT_AES           (1U << 25)
#define X86_CPUID_EXT_XSAVE         (1U << 26)
#define X86_CPUID_EXT_OSXSAVE       (1U << 27)
#define X86_CPUID_EXT_AVX           (1U << 28)
#define X86_CPUID_EXT_F16C          (1U << 29)
#define X86_CPUID_EXT_RDRAND        (1U << 30)
#define X86_CPUID_EXT_HYPERVISOR    (1U << 31)

/* FEAT_8000_0001_EDX */
#define X86_CPUID_EXT2_FPU          (1U << 0)
#define X86_CPUID_EXT2_VME          (1U << 1)
#define X86_CPUID_EXT2_DE           (1U << 2)
#define X86_CPUID_EXT2_PSE          (1U << 3)
#define X86_CPUID_EXT2_TSC          (1U << 4)
#define X86_CPUID_EXT2_MSR          (1U << 5)
#define X86_CPUID_EXT2_PAE          (1U << 6)
#define X86_CPUID_EXT2_MCE          (1U << 7)
#define X86_CPUID_EXT2_CX8          (1U << 8)
#define X86_CPUID_EXT2_APIC         (1U << 9)
#define X86_CPUID_EXT2_SYSCALL      (1U << 11)
#define X86_CPUID_EXT2_MTRR         (1U << 12)
#define X86_CPUID_EXT2_PGE          (1U << 13)
#define X86_CPUID_EXT2_MCA          (1U << 14)
#define X86_CPUID_EXT2_CMOV         (1U << 15)
#define X86_CPUID_EXT2_PAT          (1U << 16)
#define X86_CPUID_EXT2_PSE36        (1U << 17)
#define X86_CPUID_EXT2_MP           (1U << 19)
#define X86_CPUID_EXT2_NX           (1U << 20)
#define X86_CPUID_EXT2_MMXEXT       (1U << 22)
#define X86_CPUID_EXT2_MMX          (1U << 23)
#define X86_CPUID_EXT2_FXSR         (1U << 24)
#define X86_CPUID_EXT2_FFXSR        (1U << 25)
#define X86_CPUID_EXT2_PDPE1GB      (1U << 26)
#define X86_CPUID_EXT2_RDTSCP       (1U << 27)
#define X86_CPUID_EXT2_LM           (1U << 29)
#define X86_CPUID_EXT2_3DNOWEXT     (1U << 30)
#define X86_CPUID_EXT2_3DNOW        (1U << 31)


/* CPUID[8000_0001].EDX bits that are aliase of CPUID[1].EDX bits on AMD CPUs */
#define X86_CPUID_EXT2_AMD_ALIASES (CPUID_EXT2_FPU  | CPUID_EXT2_VME    | \
                                CPUID_EXT2_DE   | CPUID_EXT2_PSE    | \
                                CPUID_EXT2_TSC  | CPUID_EXT2_MSR    | \
                                CPUID_EXT2_PAE  | CPUID_EXT2_MCE    | \
                                CPUID_EXT2_CX8  | CPUID_EXT2_APIC   | \
                                CPUID_EXT2_MTRR | CPUID_EXT2_PGE    | \
                                CPUID_EXT2_MCA  | CPUID_EXT2_CMOV   | \
                                CPUID_EXT2_PAT  | CPUID_EXT2_PSE36  | \
                                CPUID_EXT2_MMX  | CPUID_EXT2_FXSR)



#define X86_CPUID_EXT3_LAHF_LM      (1U << 0)
#define X86_CPUID_EXT3_CMP_LEG      (1U << 1)
#define X86_CPUID_EXT3_SVM          (1U << 2)
#define X86_CPUID_EXT3_EXTAPIC      (1U << 3)
#define X86_CPUID_EXT3_CR8LEG       (1U << 4)
#define X86_CPUID_EXT3_ABM          (1U << 5)
#define X86_CPUID_EXT3_SSE4A        (1U << 6)
#define X86_CPUID_EXT3_MISALIGNSSE  (1U << 7)
#define X86_CPUID_EXT3_3DNOWPREFETCH (1U << 8)
#define X86_CPUID_EXT3_OSVW         (1U << 9)
#define X86_CPUID_EXT3_IBS          (1U << 10)
#define X86_CPUID_EXT3_XOP          (1U << 11)
#define X86_CPUID_EXT3_SKINIT       (1U << 12)
#define X86_CPUID_EXT3_WDT          (1U << 13)
#define X86_CPUID_EXT3_LWP          (1U << 15)
#define X86_CPUID_EXT3_FMA4         (1U << 16)
#define X86_CPUID_EXT3_TCE          (1U << 17)
#define X86_CPUID_EXT3_NODEID       (1U << 19)
#define X86_CPUID_EXT3_TBM          (1U << 21)
#define X86_CPUID_EXT3_TOPOEXT      (1U << 22)
#define X86_CPUID_EXT3_PERFCORE     (1U << 23)
#define X86_CPUID_EXT3_PERFNB       (1U << 24)

#define X86_CPUID_SVM_NPT           (1U << 0)
#define X86_CPUID_SVM_LBRV          (1U << 1)
#define X86_CPUID_SVM_SVMLOCK       (1U << 2)
#define X86_CPUID_SVM_NRIPSAVE      (1U << 3)
#define X86_CPUID_SVM_TSCSCALE      (1U << 4)
#define X86_CPUID_SVM_VMCBCLEAN     (1U << 5)
#define X86_CPUID_SVM_FLUSHASID     (1U << 6)
#define X86_CPUID_SVM_DECODEASSIST  (1U << 7)
#define X86_CPUID_SVM_PAUSEFILTER   (1U << 10)
#define X86_CPUID_SVM_PFTHRESHOLD   (1U << 12)

/* Support RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE */
#define X86_CPUID_7_0_EBX_FSGSBASE          (1U << 0)
/* Support IA32_TSC_ADJUST_MSR */
#define X86_CPUID_7_0_EBX_TSC_ADJUST        (1U << 1)
/* 1st Group of Advanced Bit Manipulation Extensions */
#define X86_CPUID_7_0_EBX_BMI1              (1U << 3)
/* Hardware Lock Elision */
#define X86_CPUID_7_0_EBX_HLE               (1U << 4)
/* Intel Advanced Vector Extensions 2 */
#define X86_CPUID_7_0_EBX_AVX2              (1U << 5)
/* Supervisor-mode Execution Prevention */
#define X86_CPUID_7_0_EBX_SMEP              (1U << 7)
/* 2nd Group of Advanced Bit Manipulation Extensions */
#define X86_CPUID_7_0_EBX_BMI2              (1U << 8)
/* Enhanced REP MOVSB/STOSB */
#define X86_CPUID_7_0_EBX_ERMS              (1U << 9)
/* Invalidate Process-Context Identifier */
#define X86_CPUID_7_0_EBX_INVPCID           (1U << 10)
/* Restricted Transactional Memory */
#define X86_CPUID_7_0_EBX_RTM               (1U << 11)
/* Memory Protection Extension */
#define X86_CPUID_7_0_EBX_MPX               (1U << 14)
/* AVX-512 Foundation */
#define X86_CPUID_7_0_EBX_AVX512F           (1U << 16)
/* AVX-512 Doubleword & Quadword Instruction */
#define X86_CPUID_7_0_EBX_AVX512DQ          (1U << 17)
/* Read Random SEED */
#define X86_CPUID_7_0_EBX_RDSEED            (1U << 18)
/* ADCX and ADOX instructions */
#define X86_CPUID_7_0_EBX_ADX               (1U << 19)
/* Supervisor Mode Access Prevention */
#define X86_CPUID_7_0_EBX_SMAP              (1U << 20)
/* AVX-512 Integer Fused Multiply Add */
#define X86_CPUID_7_0_EBX_AVX512IFMA        (1U << 21)
/* Persistent Commit */
#define X86_CPUID_7_0_EBX_PCOMMIT           (1U << 22)
/* Flush a Cache Line Optimized */
#define X86_CPUID_7_0_EBX_CLFLUSHOPT        (1U << 23)
/* Cache Line Write Back */
#define X86_CPUID_7_0_EBX_CLWB              (1U << 24)
/* Intel Processor Trace */
#define X86_CPUID_7_0_EBX_INTEL_PT          (1U << 25)
/* AVX-512 Prefetch */
#define X86_CPUID_7_0_EBX_AVX512PF          (1U << 26)
/* AVX-512 Exponential and Reciprocal */
#define X86_CPUID_7_0_EBX_AVX512ER          (1U << 27)
/* AVX-512 Conflict Detection */
#define X86_CPUID_7_0_EBX_AVX512CD          (1U << 28)
/* SHA1/SHA256 Instruction Extensions */
#define X86_CPUID_7_0_EBX_SHA_NI            (1U << 29)
/* AVX-512 Byte and Word Instructions */
#define X86_CPUID_7_0_EBX_AVX512BW          (1U << 30)
/* AVX-512 Vector Length Extensions */
#define X86_CPUID_7_0_EBX_AVX512VL          (1U << 31)

/* AVX-512 Vector Byte Manipulation Instruction */
#define X86_CPUID_7_0_ECX_AVX512_VBMI       (1U << 1)
/* User-Mode Instruction Prevention */
#define X86_CPUID_7_0_ECX_UMIP              (1U << 2)
/* Protection Keys for User-mode Pages */
#define X86_CPUID_7_0_ECX_PKU               (1U << 3)
/* OS Enable Protection Keys */
#define X86_CPUID_7_0_ECX_OSPKE             (1U << 4)
/* UMONITOR/UMWAIT/TPAUSE Instructions */
#define X86_CPUID_7_0_ECX_WAITPKG           (1U << 5)
/* Additional AVX-512 Vector Byte Manipulation Instruction */
#define X86_CPUID_7_0_ECX_AVX512_VBMI2      (1U << 6)
/* Galois Field New Instructions */
#define X86_CPUID_7_0_ECX_GFNI              (1U << 8)
/* Vector AES Instructions */
#define X86_CPUID_7_0_ECX_VAES              (1U << 9)
/* Carry-Less Multiplication Quadword */
#define X86_CPUID_7_0_ECX_VPCLMULQDQ        (1U << 10)
/* Vector Neural Network Instructions */
#define X86_CPUID_7_0_ECX_AVX512VNNI        (1U << 11)
/* Support for VPOPCNT[B,W] and VPSHUFBITQMB */
#define X86_CPUID_7_0_ECX_AVX512BITALG      (1U << 12)
/* POPCNT for vectors of DW/QW */
#define X86_CPUID_7_0_ECX_AVX512_VPOPCNTDQ  (1U << 14)
/* 5-level Page Tables */
#define X86_CPUID_7_0_ECX_LA57              (1U << 16)
/* Read Processor ID */
#define X86_CPUID_7_0_ECX_RDPID             (1U << 22)
/* Cache Line Demote Instruction */
#define X86_CPUID_7_0_ECX_CLDEMOTE          (1U << 25)
/* Move Doubleword as Direct Store Instruction */
#define X86_CPUID_7_0_ECX_MOVDIRI           (1U << 27)
/* Move 64 Bytes as Direct Store Instruction */
#define X86_CPUID_7_0_ECX_MOVDIR64B         (1U << 28)

/* AVX512 Neural Network Instructions */
#define X86_CPUID_7_0_EDX_AVX512_4VNNIW     (1U << 2)
/* AVX512 Multiply Accumulation Single Precision */
#define X86_CPUID_7_0_EDX_AVX512_4FMAPS     (1U << 3)
/* Speculation Control */
#define X86_CPUID_7_0_EDX_SPEC_CTRL         (1U << 26)
/* Single Thread Indirect Branch Predictors */
#define X86_CPUID_7_0_EDX_STIBP             (1U << 27)
/* Arch Capabilities */
#define X86_CPUID_7_0_EDX_ARCH_CAPABILITIES (1U << 29)
/* Core Capability */
#define X86_CPUID_7_0_EDX_CORE_CAPABILITY   (1U << 30)
/* Speculative Store Bypass Disable */
#define X86_CPUID_7_0_EDX_SPEC_CTRL_SSBD    (1U << 31)

/* AVX512 BFloat16 Instruction */
#define X86_CPUID_7_1_EAX_AVX512_BF16       (1U << 5)


#define X86_CPUID_8000_0008_EBX_IBPB        (1U << 12)


/* CLZERO instruction */
#define X86_CPUID_8000_0008_EBX_CLZERO      (1U << 0)
/* Always save/restore FP error pointers */
#define X86_CPUID_8000_0008_EBX_XSAVEERPTR  (1U << 2)
/* Write back and do not invalidate cache */
#define X86_CPUID_8000_0008_EBX_WBNOINVD    (1U << 9)
/* Indirect Branch Prediction Barrier */
#define X86_CPUID_8000_0008_EBX_IBPB        (1U << 12)






#define X86_CPUID_XSAVE_XSAVEOPT        (1U << 0)
#define X86_CPUID_XSAVE_XSAVEC          (1U << 1)
#define X86_CPUID_XSAVE_XGETBV1         (1U << 2)
#define X86_CPUID_XSAVE_XSAVES          (1U << 3)

#define X86_CPUID_6_EAX_ARAT            (1U << 2)



/* CPUID[0x80000007].EDX flags: */
#define X86_CPUID_APM_INVTSC            (1U << 8)



#define X86_CPUID_MWAIT_IBE             (1U << 1) /* Interrupts can exit capability */
#define X86_CPUID_MWAIT_EMX             (1U << 0) /* enumeration supported */

/* CPUID[0xB].ECX level types */
#define X86_CPUID_TOPOLOGY_LEVEL_INVALID  (0U << 8)
#define X86_CPUID_TOPOLOGY_LEVEL_SMT      (1U << 8)
#define X86_CPUID_TOPOLOGY_LEVEL_CORE     (2U << 8)
#define X86_CPUID_TOPOLOGY_LEVEL_DIE      (5U << 8)








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
            "pushf; pop rax"
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
        __asm__ __volatile__ ("mov" #p " %%" #e ", %0" :: "r"(r));              \
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
        "lfence; rdtsc; lfence\n" 
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
    );
}

/*!
 * @brief Read GS Base Register.
 */
static inline unsigned long long x86_rdgsbase() {
    unsigned long long r;
    __asm__ __volatile__ (
        "rdgsbase %0"
        :
        : "m"(r)
    );
    
    return r;
}

/*!
 * @brief Read FS Base Register.
 */
static inline unsigned long long x86_rdfsbase() {
    unsigned long long r;
    __asm__ __volatile__ (
        "rdfsbase %0"
        :
        : "m"(r)
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


#endif


__BEGIN_DECLS

__END_DECLS

#endif