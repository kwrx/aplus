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


/* OS */
#ifdef APLUS
EXTERN int aplus_thread_create(uint32_t entry, void* param, int priority);
EXTERN void aplus_thread_idle();
EXTERN void aplus_thread_wakeup();

/* unistd.h "kill(pid, sig)" */
#define aplus_thread_kill(tid, sig) kill(tid, sig)

#endif

PRIVATE inline int __os_thread_create(uint32_t entry, void* param, int priority) {
#ifdef APLUS
	return aplus_thread_create(entry, param, priority);
#else
	errno = ENOSYS;
	return 1;
#endif
}

PRIVATE inline void __os_thread_idle() {
#ifdef APLUS
	aplus_thread_idle();
#else
	errno = ENOSYS;
#endif
}


PRIVATE inline void __os_thread_wakeup() {
#ifdef APLUS
	aplus_thread_wakeup();
#else
	errno = ENOSYS;
#endif
}

PRIVATE inline int __os_thread_kill(int tid, int sig) {
#ifdef APLUS
	aplus_thread_kill(tid, sig);
#else
	errno = ENOSYS;
	return 1;
#endif
}


#endif