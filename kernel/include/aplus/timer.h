#ifndef _TIMER_H
#define _TIMER_H


#ifndef __ASSEMBLY__
#include <stdint.h>
typedef uint64_t ktime_t;

ktime_t timer_gettime();
ktime_t timer_getticks();
ktime_t timer_getms();
ktime_t timer_getfreq();
void timer_delay(ktime_t ms);

#ifdef CLOCKS_PER_SEC
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC			(timer_getfreq())
#endif

#endif



#endif
