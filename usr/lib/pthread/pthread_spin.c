
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
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
	if(!lock) {
		errno = EINVAL;
		return 1;
	}

	lock->interlock = 0;
	pthread_mutex_init(&lock->mutex, NULL);

	return 0;
}

PUBLIC int pthread_spin_destroy(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return 1;
	}

	lock->interlock = 0;
	pthread_mutex_destroy(&lock->mutex);

	return 0;
}

PUBLIC int pthread_spin_lock(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return 1;
	}

	if(pthread_mutex_lock(&lock->mutex) == 0)
		lock->interlock = 1;
	else
		return 1; /* errno already setted */

	return 0;
}

PUBLIC int pthread_spin_trylock(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return 1;
	}

	if(pthread_mutex_trylock(&lock->mutex) == 0)
		lock->interlock = 1;
	else
		return 1; /* errno already setted */

	return 0;
}

PUBLIC int pthread_spin_unlock(pthread_spinlock_t *lock) {
	if(!lock) {
		errno = EINVAL;
		return 1;
	}

	if(pthread_mutex_unlock(&lock->mutex) == 0)
		lock->interlock = 0;
	else
		return 1; /* errno already setted */
	
	return 0;
}