#ifndef _ARCH_X86_CPU_H
#define _ARCH_X86_CPU_H


/*!
 * @brief Get x86 Register value.
 */
#define x86_get_reg(p, e)                                                       \
    static inline long x86_get_##e() {                                          \
        long r;                                                                 \
        __asm__ __volatile__ ("mov" #p " %" #e ", %%0" : "=r"(r));              \
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
        __asm__ __volatile__ ("mov" #p " %" #e ", %%0" :: "r"(r));              \
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
            "pushq rax; popf"
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
static inline void x86_cpuid(long r, long* a, long* d) {
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(*a), "=d"(*d) : "a"(r) 
        : "rcx", "rbx"
    );
}

#endif