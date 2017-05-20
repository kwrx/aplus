#ifndef _TIMER_H
#define _TIMER_H


#ifndef __ASSEMBLY__
#include <stdint.h>
typedef uint64_t ktime_t;

ktime_t timer_gettimestamp();
ktime_t timer_getticks();
ktime_t timer_getms();
ktime_t timer_getus();
ktime_t timer_getfreq();
void timer_delay(ktime_t ms);


#endif



#endif
