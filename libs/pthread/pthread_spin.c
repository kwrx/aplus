
#include <stdint.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
	if(!lock) {
		errno = EINVAL;
		return -1;
	}

	lock->interlock = 0;
	pthread_mutex_init(&lock->mutex, NULL);

	return 0;
}

PUBLIC int pthread_spin_destroy(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return -1;
	}

	lock->interlock = 0;
	pthread_mutex_destroy(&lock->mutex);

	return 0;
}

PUBLIC int pthread_spin_lock(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return -1;
	}

	if(pthread_mutex_lock(&lock->mutex) == 0)
		lock->interlock = 1;
	else
		return -1; /* errno already setted */

	return 0;
}

PUBLIC int pthread_spin_trylock(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return -1;
	}

	if(pthread_mutex_trylock(&lock->mutex) == 0)
		lock->interlock = 1;
	else
		return -1; /* errno already setted */

	return 0;
}

PUBLIC int pthread_spin_unlock(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return -1;
	}

	if(pthread_mutex_unlock(&lock->mutex) == 0)
		lock->interlock = 0;
	else
		return -1; /* errno already setted */
	
	return 0;
}
