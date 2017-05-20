
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>

#include "pthread_internal.h"


PUBLIC int pthread_rwlock_init(pthread_rwlock_t *lock, const pthread_rwlockattr_t *attr) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	pthread_t thread = pthread_self();
	if(!thread) {
		errno = EFAULT;
		return -1;
	}

	pthread_mutex_init(&lock->rdmutex, NULL);
	pthread_mutex_init(&lock->wrmutex, NULL);

	lock->shared_waiters = (handle_t) 0;
	lock->exclusive_waiters = (handle_t) 0;
	lock->num_shared_waiters = 0;
	lock->num_exclusive_waiters = 0;
	lock->owner = thread;

	return 0;
}

PUBLIC int pthread_rwlock_destroy(pthread_rwlock_t *lock) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	pthread_t thread = pthread_self();
	if(!thread) {
		errno = EFAULT;
		return -1;
	}

	if(thread != lock->owner) {
		errno = EPERM;
		return -1;
	}

	pthread_mutex_destroy(&lock->rdmutex);
	pthread_mutex_destroy(&lock->wrmutex);

	lock->shared_waiters = (handle_t) 0;
	lock->exclusive_waiters = (handle_t) 0;
	lock->num_shared_waiters = 0;
	lock->num_exclusive_waiters = 0;
	lock->owner = 0;
	
	return 0;
}

PUBLIC int pthread_rwlock_tryrdlock(pthread_rwlock_t *lock) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	return pthread_mutex_trylock(&lock->rdmutex);
}

PUBLIC int pthread_rwlock_trywrlock(pthread_rwlock_t *lock) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	return pthread_mutex_trylock(&lock->wrmutex);
}

PUBLIC int pthread_rwlock_rdlock(pthread_rwlock_t *lock) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	return pthread_mutex_lock(&lock->rdmutex);
}

PUBLIC int pthread_rwlock_timedrdlock(pthread_rwlock_t *lock, const struct timespec *abstime) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	return pthread_mutex_timedlock(&lock->rdmutex, abstime);
}

PUBLIC int pthread_rwlock_wrlock(pthread_rwlock_t *lock) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	return pthread_mutex_lock(&lock->wrmutex);
}

PUBLIC int pthread_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *abstime) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	return pthread_mutex_timedlock(&lock->wrmutex, abstime);
}

PUBLIC int pthread_rwlock_unlock(pthread_rwlock_t *lock) {
	if(lock == NULL) {
		errno = EINVAL;
		return -1;
	}

	if(pthread_mutex_unlock(&lock->rdmutex) != 0)
		return -1;

	if(pthread_mutex_unlock(&lock->wrmutex) != 0)
		return -1;

	return 0;
}
