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


#endif
#endif