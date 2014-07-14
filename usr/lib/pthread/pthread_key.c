
//
//  pthread_key.c
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

PRIVATE __pthread_key_t __pthread_keys[PTHREAD_KEYS_MAX] = { 0 };

PUBLIC int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
	if(!key) {
		errno = EINVAL;
		return 1;
	}

	for(int i = 0; i < PTHREAD_KEYS_MAX; i++) {
		if(__pthread_keys[i].used == 0) {
			__pthread_keys[i].used = 1;
			__pthread_keys[i].dtor = destructor;

			*key = i;
			return 0;
		}
	}

	errno = EAGAIN;
	return 1;
}


PUBLIC int pthread_key_delete(pthread_key_t key) {
	if(key > PTHREAD_KEYS_MAX) {
		errno = EINVAL;
		return 1;
	}

	__pthread_keys[key].used = 0;
	__pthread_keys[key].dtor = 0;
	
	return 0;
}


PUBLIC int pthread_setspecific(pthread_key_t key, const void *value) {
	if(key > PTHREAD_KEYS_MAX) {
		errno = EINVAL;
		return 1;
	}

	if(__pthread_keys[key].used == 0) {
		errno = EINVAL;
		return 1;
	}

	pthread_t ptx = pthread_self();
	if(!ptx) {
		errno = ESRCH;
		return 1;
	}

	pthread_context_t* ctx = (pthread_context_t*) ptx;
	ctx->keys[key] = (pthread_key_t) value;

	return 0;
}


PUBLIC void *pthread_getspecific(pthread_key_t key) {
	if(key > PTHREAD_KEYS_MAX) {
		errno = EINVAL;
		return 0;
	}

	if(__pthread_keys[key].used == 0) {
		errno = EINVAL;
		return 0;
	}

	pthread_t ptx = pthread_self();
	if(!ptx) {
		errno = ESRCH;
		return 0;
	}

	pthread_context_t* ctx = (pthread_context_t*) ptx;
	return ctx->keys[key];
}