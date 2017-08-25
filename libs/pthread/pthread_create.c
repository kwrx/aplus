
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "pthread_internal.h"


EXTERN uint32_t __pthread_counts;
EXTERN pthread_context_t* __pthread_queue;

EXTERN void __pthread_init_queue(void);


PRIVATE void __pthread_handler__(pthread_context_t* ctx) {
	if(ctx == NULL)
		_exit(-1);

	if(ctx->entry == NULL)
		_exit(-1);

	pthread_exit(ctx->entry(ctx->param));
}


PUBLIC int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void *(*start)(void*), void* arg) {

	__pthread_init_queue();

	if(!thread) {
		errno = EINVAL;
		return -1;
	}

	if(__pthread_counts > PTHREAD_THREADS_MAX) {
		errno = EAGAIN;
		return -1;
	}

	pthread_context_t* ctx = malloc(sizeof(pthread_context_t));
	memset(ctx, 0, sizeof(pthread_context_t));

	ctx->entry = start;
	ctx->param = arg;
	ctx->once.done = 0;
	ctx->once.started = -1;
	ctx->next = 0;

	if(attr)
		memcpy(&ctx->attr, attr, sizeof(pthread_attr_t));

	if(ctx->attr.inheritsched == PTHREAD_INHERIT_SCHED) {
		pthread_t px = pthread_self();
		if(px) {
			pthread_context_t* inh = (pthread_context_t*) px;
			pthread_attr_setschedparam(&ctx->attr, &inh->attr.param);
		}
	}


	__pthread_counts += 1;
	*thread = (pthread_t) ctx;
	
	
	ctx->next = __pthread_queue;
	__pthread_queue = ctx;
		

	/* FIXME */
	extern pid_t clone(void* (*fn) (void*), void*, int, void*);
	ctx->tid = clone((void* (*)(void*)) __pthread_handler__, NULL, CLONE_SIGHAND | CLONE_FILES | CLONE_FS | CLONE_VM, ctx);
	return 0;
}
