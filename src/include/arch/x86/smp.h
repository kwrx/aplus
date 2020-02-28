#ifndef _APLUS_X86_APIC_H
#define _APLUS_X86_APIC_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>


typedef struct {
    uint64_t magic;
    uint64_t entrypoint;
    uint64_t cr3;
    uint64_t stack;
    uint64_t gdt64;
} ap_header_t;


__BEGIN_DECLS

void ap_init(void);

__END_DECLS

#endif
#endif