#ifndef _APLUS_HAL_TIMER_H
#define _APLUS_HAL_TIMER_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>
#include <aplus/debug.h>

__BEGIN_DECLS

void arch_timer_delay(uint64_t);
uint64_t arch_timer_gettime(void);

uint64_t arch_timer_percpu_getticks(void);
uint64_t arch_timer_percpu_getns(void);
uint64_t arch_timer_percpu_getus(void);
uint64_t arch_timer_percpu_getms(void);
uint64_t arch_timer_percpu_getres(void);

uint64_t arch_timer_generic_getticks(void);
uint64_t arch_timer_generic_getns(void);
uint64_t arch_timer_generic_getus(void);
uint64_t arch_timer_generic_getms(void);
uint64_t arch_timer_generic_getres(void);

__END_DECLS

#endif
#endif