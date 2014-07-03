
//
//  pthread_self.c
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

pthread_t pthread_self(void) {
	if(!__pthread_queue) {
		errno = ESRCH;
		return 0;
	}

	int pid = getpid();
	pthread_context_t* tmp = __pthread_queue;
	while(tmp) {
		if(tmp->tid == pid)
			return (pthread_t) tmp;

		tmp = tmp->next;
	}

	return 0;
}