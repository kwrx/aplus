#ifndef _APLUS_X86_INTR_H
#define _APLUS_X86_INTR_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>



typedef struct {

    uint64_t di;
    uint64_t si;
    uint64_t bp;
    uint64_t bx;
    uint64_t dx;
    uint64_t cx;
    uint64_t ax;

    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    uint64_t intno;
    uint64_t errno;

    uint64_t ip;
    uint64_t cs;
    uint64_t flags;

    uint64_t user_sp;
    uint64_t user_ss;

} interrupt_frame_t;

__BEGIN_DECLS

void timer_init();

__END_DECLS

#endif
#endif