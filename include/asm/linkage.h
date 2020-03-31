#ifndef _APLUS_LINKAGE_H
#define _APLUS_LINKAGE_H

#ifdef __cplusplus
#define asmlinkage extern "C"
#else
#define asmlinkage
#endif


#ifdef __ASSEMBLY__

#define ENTRY(proc)             \
    .globl proc;                \
    .align 16, 0x90             \
    proc:

#define END(proc)               \
    .size proc, . - proc

#define ENDPROC(proc)           \
    .type proc, @function;      \
    END(proc)


#if defined(__x86_64__)
#define STDCALL(func)           \
    pushq   %rbp;               \
    movq    %rsp, %rbp;         \
    callq   func;               \
    popq    %rbp;
#endif


#endif
#endif