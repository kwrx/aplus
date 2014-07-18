#ifndef PTHREAD_OS_H
#define PTHREAD_OS_H

#include "pthread_internal.h"

#if defined (APLUS)
EXTERN int aplus_thread_create(uint32_t entry, void* param, int priority);
EXTERN void aplus_thread_idle();
EXTERN void aplus_thread_wakeup();


/* Hooks */
#define __os_thread_create(e, a, p)			aplus_thread_create(e, a, p)
#define __os_thread_idle()					__idle()
#define __os_thread_wakeup()				__wakeup()
#define __os_thread_kill(t, s)				kill(t, s)
#define __os_gettid()						getpid()


// #elif defined (__ANOTHER_OS_)
// bla bla bla...

#else
#error "Undefinited OS"
#endif




#endif