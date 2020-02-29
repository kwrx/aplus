#ifndef _APLUS_X86_SMP_H
#define _APLUS_X86_SMP_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>


typedef struct {
    uint64_t magic;
    uint64_t entry;
    uint64_t cr3;
    uint64_t stack;
    uint64_t gdt64;
} ap_header_t;


__BEGIN_DECLS

void ap_main(void);
void ap_init(void);
int ap_check(int, int);
ap_header_t* ap_get_header(void);

__END_DECLS

#endif
#endif