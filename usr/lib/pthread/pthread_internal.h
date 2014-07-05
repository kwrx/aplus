#ifndef PTHREAD_INTERNAL_H
#define PTHREAD_INTERNAL_H

#include <errno.h>
#include <stdint.h>

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



typedef struct __pthread_key {
	char used;
	void (*dtor) (void*);
} __pthread_key_t;

typedef struct pthread_context {
	int tid;
	void *(*entry) (void* p);
	void* param;
	void* exitval;

	pthread_once_t once;
	pthread_attr_t attr;

	pthread_key_t keys[PTHREAD_KEYS_MAX];

	struct pthread_context* next;
} pthread_context_t;


#include "pthread_os.h"

#endif