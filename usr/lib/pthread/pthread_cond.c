
//
//  pthread_cond.c
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
#include <pthread.h>

#include "pthread_internal.h"

EXTERN pthread_context_t* __pthread_queue;



PUBLIC int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
	if(!cond) {
		errno = EINVAL;
		return 1;
	}

	cond->waiting = 0;
	cond->semaphore = 0;

	return 0;
}

PUBLIC int pthread_cond_destroy(pthread_cond_t *cond) {
	if(!cond) {
		errno = EINVAL;
		return 1;
	}

	if(cond->waiting) {
		errno = EBUSY;
		return 1;
	}


	cond->waiting = 0;
	cond->semaphore = 0;		
	
	return 0;
}

PUBLIC int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
	if(!cond) {
		errno = EINVAL;
		return 1;
	}

	if(!mutex) {
		errno = EINVAL;
		return 1;
	}

	if(mutex->owner != pthread_self()) {
		errno = EINVAL;
		return 1;
	}

	pthread_context_t* ctx = (pthread_context_t*) pthread_self();
	if(!ctx) {
		errno = EINVAL;
		return -1;
	}

	ctx->cond = cond;
	ctx->cond->waiting = 1;
	ctx->cond->semaphore = 1;


	while(ctx->cond->semaphore)
		__os_thread_yield();

	ctx->cond->waiting = 0;
	ctx->cond->semaphore = 0;
	ctx->cond = 0;

	pthread_mutex_unlock(mutex);

	return 0;
}

PUBLIC int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
	errno = ENOSYS;
	return 1;
}


PUBLIC int pthread_cond_signal(pthread_cond_t *cond) {
	if(!__pthread_queue) {
		errno = ESRCH;
		return -1;
	}


	pthread_context_t* tmp = __pthread_queue;
	while(tmp) {
		if(tmp->cond == cond) {
			if(tmp->cond->waiting) {
				tmp->cond->semaphore = 0;
				return 0;			
			}
		}

		tmp = tmp->next;
	}

	errno = ESRCH;
	return -1;
}

PUBLIC int pthread_cond_broadcast(pthread_cond_t *cond) {
	if(!__pthread_queue) {
		errno = ESRCH;
		return -1;
	}


	pthread_context_t* tmp = __pthread_queue;
	while(tmp) {
		if(tmp->cond == cond) {
			if(tmp->cond->waiting) {
				tmp->cond->semaphore = 0;		
			}
		}

		tmp = tmp->next;
	}


	return 0;
}