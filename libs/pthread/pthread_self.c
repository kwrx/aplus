
#include <stdint.h>
#include <pthread.h>

#include "pthread_internal.h"


EXTERN pthread_context_t* __pthread_queue;

PUBLIC pthread_t pthread_self(void) {
	if(!__pthread_queue) {
		errno = ESRCH;
		return -1;
	}

	int tid = getpid();
	pthread_context_t* tmp = __pthread_queue;
	while(tmp) {
		if(tmp->tid == tid)
			return (pthread_t) tmp;

		tmp = tmp->next;
	}

	errno = ESRCH;
	return -1;
}
