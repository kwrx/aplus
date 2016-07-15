#ifndef _TIMER_H
#define _TIMER_H


#ifndef __ASSEMBLY__
long timer_gettime();
long timer_getticks();
long timer_getms();
long timer_getfreq();
void timer_delay(long ms);

#ifdef CLOCKS_PER_SEC
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC			(timer_getfreq())
#endif

#endif



#endif
