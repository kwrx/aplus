
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


PUBLIC int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
	if(!attr) {
		errno = EINVAL;
		return 1;
	}

	attr->pshared = 0;
	attr->kind = PTHREAD_MUTEX_DEFAULT;
	
	return 0;
}

PUBLIC int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
	if(!attr) {
		errno = EINVAL;
		return 1;
	}

	attr->pshared = 0;
	attr->kind = 0;
	
	return 0;
}

PUBLIC int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared) {
	if(!attr) {
		errno = EINVAL;
		return 1;
	}

	*pshared = attr->pshared;
	return 0;
}

PUBLIC int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared) {
	if(!attr) {
		errno = EINVAL;
		return 1;
	}

	attr->pshared = pshared;
	return 0;
}

PUBLIC int pthread_mutexattr_gettype(pthread_mutexattr_t *attr, int *kind) {
	if(!attr) {
		errno = EINVAL;
		return 1;
	}

	*kind = attr->kind;
	return 0;
}

PUBLIC int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind) {
	if(!attr) {
		errno = EINVAL;
		return 1;
	}

	attr->kind = kind;
	return 0;
}