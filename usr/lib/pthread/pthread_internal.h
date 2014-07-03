#ifndef PTHREAD_INTERNAL_H
#define PTHREAD_INTERNAL_H

#include <errno.h>

#ifndef APLUS
#error "A compatible OS is only aPlus"
#endif

#ifdef DEBUG
#define PRIVATE
#else
#define PRIVATE static
#endif


#define PUBLIC

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

#define TASK_PRIORITY_NORMAL	25


typedef struct pthread_context {
	int tid;
	void *(*entry) (void* p);
	void* param;
	void* exitval;

	pthread_once_t once;
	pthread_attr_t attr;

	struct pthread_context* next;
} pthread_context_t;


EXTERN int aplus_thread_create(uint32_t entry, void* param, int priority);
EXTERN void aplus_thread_idle();
EXTERN void aplus_thread_wakeup();
EXTERN void aplus_thread_zombie();

#endif