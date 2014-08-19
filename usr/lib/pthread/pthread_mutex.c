
//
//  pthread_mutexattr.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdint.h>
#include <sys/time.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
	if(!mutex) {
		errno = EINVAL;
		return 1;
	}

	mutex->lock = 0;
	mutex->recursion = 0;
	mutex->kind = PTHREAD_MUTEX_DEFAULT;
	mutex->owner = 0;
	mutex->event = 0;

	if(attr)
		mutex->kind = attr->kind;

	return 0;
}


PUBLIC int pthread_mutex_destroy(pthread_mutex_t *mutex) {
	if(!mutex) {
		errno = EINVAL;
		return 1;
	}

	if(mutex->lock == 1) {
		errno = EBUSY;
		return 1;
	}

	mutex->lock = 0;
	mutex->recursion = 0;
	mutex->kind = 0;
	mutex->owner = 0;
	mutex->event = 0;

	return 0;
}

PUBLIC int pthread_mutex_lock(pthread_mutex_t *mutex) {
	if(!mutex) {
		errno = EINVAL;
		return 1;
	}

	if(mutex->owner != pthread_self()) {
		while(mutex->lock != 0)
			__os_thread_yield();
		
		mutex->owner = pthread_self();
		mutex->recursion = 0;
		mutex->lock = 1;
	} else if(mutex->kind == PTHREAD_MUTEX_ERRORCHECK) {
		errno = EDEADLK;
		return 1;
	}

	if(mutex->kind == PTHREAD_MUTEX_RECURSIVE)
		mutex->recursion += 1;

	return 0;
}


PUBLIC int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime) {
	errno = ENOSYS;
	return 1;
}


PUBLIC int pthread_mutex_trylock(pthread_mutex_t *mutex) {
	if(!mutex) {
		errno = EINVAL;
		return 1;
	}

	if(mutex->owner != pthread_self()) {
		if(mutex->lock != 0) {
			errno = EBUSY;
			return 1;
		}

		mutex->owner = pthread_self();
		mutex->recursion = 0;
		mutex->lock = 1;
	} else if(mutex->kind == PTHREAD_MUTEX_ERRORCHECK) {
		errno = EDEADLK;
		return 1;
	}

	if(mutex->kind == PTHREAD_MUTEX_RECURSIVE)
		mutex->recursion += 1;

	return 0;
}

PUBLIC int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	if(!mutex) {
		errno = EINVAL;
		return 1;
	}

	if(mutex->owner == pthread_self()) {
		if(mutex->kind == PTHREAD_MUTEX_RECURSIVE) {
			if(--(mutex->recursion))
				return 0;
		}

		mutex->owner = 0;
		mutex->lock = 0;
	} else if(mutex->kind == PTHREAD_MUTEX_ERRORCHECK) {
		errno = EPERM;
		return 1;
	}

	return 0;
}
