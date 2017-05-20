
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

#include "pthread_internal.h"


EXTERN uint32_t __pthread_counts;
EXTERN pthread_context_t* __pthread_queue;

PUBLIC int pthread_detach(pthread_t thread) {
	if(!thread) {
		errno = EINVAL;
		return -1;
	}

	if(!__pthread_queue) {
		errno = ESRCH;
		return -1;
	}

	pthread_context_t* ctx = (pthread_context_t*) thread;
	if(kill(ctx->tid, SIGTERM) == 0)
		ctx->once.done = 1;
	else {
		errno = ESRCH;
		return -1;
	}

	__pthread_counts -= 1;
	pthread_context_t* tmp = __pthread_queue;


	struct pthread_cleanup* ct;
	for(ct = tmp->cleanup_handlers; ct; ct = ct->next)
		ct->routine(ct->arg);



	if(tmp->next) {
		while(tmp->next) {
			if(tmp->next == ctx) {
				tmp->next = ctx->next;
				break;
			}

			tmp = tmp->next;
		}
	} else {
		if(__pthread_queue == ctx)
			__pthread_queue = NULL;
	}

	return 0;
}
