#ifndef _APLUS_HAL_INTERRUPT_H
#define _APLUS_HAL_INTERRUPT_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>


typedef void (*irq_t) (void*);


__BEGIN_DECLS

void arch_intr_enable(long);
long arch_intr_disable(void);

__END_DECLS

#endif
#endif
