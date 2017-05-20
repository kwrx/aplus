#ifndef PTHREAD_INTERNAL_H
#define PTHREAD_INTERNAL_H

#include <errno.h>
#include <stdint.h>
#include <sched.h>
#include <pthread.h>

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



#define PTHREAD_KEYS_MAX		64
#define PTHREAD_STACK_MIN		4096
#define PTHREAD_THREADS_MAX		2048



typedef struct __pthread_key {
	char used;
	void (*dtor) (void*);
	void* value;
} __pthread_key_t;


struct pthread_cleanup {
	void (*routine) (void*);
	void* arg;
	struct pthread_cleanup* next;
};

typedef struct pthread_context {
	int tid;
	void *(*entry) (void* p);
	void* param;
	void* exitval;

	pthread_once_t once;
	pthread_attr_t attr;
	pthread_cond_t* cond;

	struct pthread_cleanup* cleanup_handlers;
	struct pthread_context* next;
} pthread_context_t;


extern int sched_yield(void);


#endif
