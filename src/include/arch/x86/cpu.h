#ifndef _ARCH_X86_CPU_H
#define _ARCH_X86_CPU_H


#define X86_MSR_EFER                                 (0xC0000080)
#define X86_MSR_FSBASE                               (0xC0000100)
#define X86_MSR_GSBASE                               (0xC0000101)
#define X86_MSR_KERNEL_GSBASE                        (0xC0000102)


#define X86_MSR_FEATURES_SCE                         (1 << 0)
#define X86_MSR_FEATURES_LME                         (1 << 8)
#define X86_MSR_FEATURES_LMA                         (1 << 10)
#define X86_MSR_FEATURES_NXE                         (1 << 11)
#define X86_MSR_FEATURES_SVME                        (1 << 12)
#define X86_MSR_FEATURES_LMSLE                       (1 << 13)
#define X86_MSR_FEATURES_FFXSR                       (1 << 14)
#define X86_MSR_FEATURES_TCE                         (1 << 15)


// ECX
#define X86_CPU_FEATURES_SSE3                        (1ULL << 0)    // Streaming SIMD Extensions 3
#define X86_CPU_FEATURES_PCLMULQDQ                   (1ULL << 1)    // PCLMULQDQ Instruction
#define X86_CPU_FEATURES_DTES64                      (1ULL << 2)    // 64-Bit Debug Store Area
#define X86_CPU_FEATURES_MONITOR                     (1ULL << 3)    // MONITOR/MWAIT
#define X86_CPU_FEATURES_DS_CPL                      (1ULL << 4)    // CPL Qualified Debug Store
#define X86_CPU_FEATURES_VMX                         (1ULL << 5)    // Virtual Machine Extensions
#define X86_CPU_FEATURES_SMX                         (1ULL << 6)    // Safer Mode Extensions
#define X86_CPU_FEATURES_EST                         (1ULL << 7)    // Enhanced SpeedStep Technology
#define X86_CPU_FEATURES_TM2                         (1ULL << 8)    // Thermal Monitor 2
#define X86_CPU_FEATURES_SSSE3                       (1ULL << 9)    // Supplemental Streaming SIMD Extensions 3
#define X86_CPU_FEATURES_CNXT_ID                     (1ULL << 10)   // L1 Context ID
#define X86_CPU_FEATURES_FMA                         (1ULL << 12)   // Fused Multiply Add
#define X86_CPU_FEATURES_CX16                        (1ULL << 13)   // CMPXCHG16B Instruction
#define X86_CPU_FEATURES_XTPR                        (1ULL << 14)   // xTPR Update Control
#define X86_CPU_FEATURES_PDCM                        (1ULL << 15)   // Perf/Debug Capability MSR
#define X86_CPU_FEATURES_PCID                        (1ULL << 17)   // Process-context Identifiers
#define X86_CPU_FEATURES_DCA                         (1ULL << 18)   // Direct Cache Access
#define X86_CPU_FEATURES_SSE41                       (1ULL << 19)   // Streaming SIMD Extensions 4.1
#define X86_CPU_FEATURES_SSE42                       (1ULL << 20)   // Streaming SIMD Extensions 4.2
#define X86_CPU_FEATURES_X2APIC                      (1ULL << 21)   // Extended xAPIC Support
#define X86_CPU_FEATURES_MOVBE                       (1ULL << 22)   // MOVBE Instruction
#define X86_CPU_FEATURES_POPCNT                      (1ULL << 23)   // POPCNT Instruction
#define X86_CPU_FEATURES_APIC_TSC                    (1ULL << 24)   // Local APIC supports TSC Deadline
#define X86_CPU_FEATURES_AESNI                       (1ULL << 25)   // AESNI Instruction
#define X86_CPU_FEATURES_XSAVE                       (1ULL << 26)   // XSAVE/XSTOR States
#define X86_CPU_FEATURES_OSXSAVE                     (1ULL << 27)   // OS Enabled Extended State Management
#define X86_CPU_FEATURES_AVX                         (1ULL << 28)   // AVX Instructions
#define X86_CPU_FEATURES_F16C                        (1ULL << 29)   // 16-bit Floating Point Instructions
#define X86_CPU_FEATURES_RDRAND                      (1ULL << 30)   // RDRAND Instruction

// EDX
#define X86_CPU_FEATURES_FPU                         (1ULL << 32)   // Floating-Point Unit On-Chip
#define X86_CPU_FEATURES_VME                         (1ULL << 33)   // Virtual 8086 Mode Extensions
#define X86_CPU_FEATURES_DE                          (1ULL << 34)   // Debugging Extensions
#define X86_CPU_FEATURES_PSE                         (1ULL << 35)   // Page Size Extension
#define X86_CPU_FEATURES_TSC                         (1ULL << 36)   // Time Stamp Counter
#define X86_CPU_FEATURES_MSR                         (1ULL << 37)   // Model Specific Registers
#define X86_CPU_FEATURES_PAE                         (1ULL << 38)   // Physical Address Extension
#define X86_CPU_FEATURES_MCE                         (1ULL << 39)   // Machine-Check Exception
#define X86_CPU_FEATURES_CX8                         (1ULL << 40)   // CMPXCHG8 Instruction
#define X86_CPU_FEATURES_APIC                        (1ULL << 41)   // APIC On-Chip
#define X86_CPU_FEATURES_SEP                         (1ULL << 43)   // SYSENTER/SYSEXIT instructions
#define X86_CPU_FEATURES_MTRR                        (1ULL << 44)   // Memory Type Range Registers
#define X86_CPU_FEATURES_PGE                         (1ULL << 45)   // Page Global Bit
#define X86_CPU_FEATURES_MCA                         (1ULL << 46)   // Machine-Check Architecture
#define X86_CPU_FEATURES_CMOV                        (1ULL << 47)   // Conditional Move Instruction
#define X86_CPU_FEATURES_PAT                         (1ULL << 48)   // Page Attribute Table
#define X86_CPU_FEATURES_PSE36                       (1ULL << 49)   // 36-bit Page Size Extension
#define X86_CPU_FEATURES_PSN                         (1ULL << 50)   // Processor Serial Number
#define X86_CPU_FEATURES_CLFLUSH                     (1ULL << 51)   // CLFLUSH Instruction
#define X86_CPU_FEATURES_DS                          (1ULL << 53)   // Debug Store
#define X86_CPU_FEATURES_ACPI                        (1ULL << 54)   // Thermal Monitor and Software Clock Facilities
#define X86_CPU_FEATURES_MMX                         (1ULL << 55)   // MMX Technology
#define X86_CPU_FEATURES_FXSR                        (1ULL << 56)   // FXSAVE and FXSTOR Instructions
#define X86_CPU_FEATURES_SSE                         (1ULL << 57)   // Streaming SIMD Extensions
#define X86_CPU_FEATURES_SSE2                        (1ULL << 58)   // Streaming SIMD Extensions 2
#define X86_CPU_FEATURES_SS                          (1ULL << 59)   // Self Snoop
#define X86_CPU_FEATURES_HTT                         (1ULL << 60)   // Multi-Threading
#define X86_CPU_FEATURES_TM                          (1ULL << 61)   // Thermal Monitor
#define X86_CPU_FEATURES_PBE                         (1ULL << 63)   // Pending Break Enable

// EDX (Extended)
#define X86_CPU_XFEATURES_SYSCALL                    (1ULL << 43)   // SYSCALL/SYSRET
#define X86_CPU_XFEATURES_XD                         (1ULL << 52)   // Execute Disable Bit
#define X86_CPU_XFEATURES_1GB_PAGE                   (1ULL << 58)   // 1 GB Pages
#define X86_CPU_XFEATURES_RDTSCP                     (1ULL << 59)   // RDTSCP and IA32_TSC_AUX
#define X86_CPU_XFEATURES_64_BIT                     (1ULL << 61)   // 64-bit Architecture



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
    unsigned long long vl, vh;
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
    unsigned long long r;
    __asm__ __volatile__ (
        "lfence; rdtsc; lfence\n" 
        : "=A"(r)
    );

    return r;
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
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(r) 
    );
}


__BEGIN_DECLS

__END_DECLS

#endif