#ifndef _APLUS_X86_SMP_H
#define _APLUS_X86_SMP_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>


typedef struct {
    uint64_t magic;
    uint64_t cpu;
    uint64_t cr3;
} __packed ap_header_t;


__BEGIN_DECLS

void ap_main(void);
void ap_init(void);
int ap_check(int, int);
ap_header_t* ap_get_header(void);

__END_DECLS

#endif
#endif