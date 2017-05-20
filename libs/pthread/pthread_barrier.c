
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
	if(barrier == NULL) {
		errno = EINVAL;
		return -1;
	}

	if(count == 0) {
		errno = EINVAL;
		return -1;
	}

	barrier->curr_height = 0;
	barrier->init_height = count;
	barrier->owner = pthread_self();
	

	return 0;
}


PUBLIC int pthread_barrier_destroy(pthread_barrier_t *barrier) {
	if(barrier == NULL) {
		errno = EINVAL;
		return -1;
	}

	barrier->curr_height = 0;
	barrier->init_height = 0;
	

	return 0;
}


PUBLIC int pthread_barrier_wait(pthread_barrier_t *barrier) {
	if(barrier == NULL) {
		errno = EINVAL;
		return -1;
	}

	barrier->curr_height += 1;

	while(barrier->curr_height < barrier->init_height)
		sched_yield();

	if(pthread_self() == barrier->owner)
		return PTHREAD_BARRIER_SERIAL_THREAD;

	return 0;
}
