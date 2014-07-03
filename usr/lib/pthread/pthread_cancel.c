
//
//  pthread_cancel.c
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

int pthread_cancel(pthread_t thread) {
	return pthread_detach(thread);
}


int pthread_setcancelstate(int state, int *oldstate) {
	errno = ENOSYS;
	return 1;
}


int pthread_setcanceltype(int type, int *oldtype) {
	errno = ENOSYS;
	return 1;
}

void pthread_testcancel(void) {
	/* Boh ?? */
}