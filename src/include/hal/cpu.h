#ifndef _APLUS_HAL_CPU_H
#define _APLUS_HAL_CPU_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>



__BEGIN_DECLS

void arch_cpu_init(int);
void arch_cpu_startup(int);
uint64_t arch_cpu_get_current_id(void);

__END_DECLS

#endif
#endif
