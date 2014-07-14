
//
//  pthread_detach.c
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


EXTERN uint32_t __pthread_counts;
EXTERN pthread_context_t* __pthread_queue;

PUBLIC int pthread_detach(pthread_t thread) {
	if(!thread) {
		errno = EINVAL;
		return 1;
	}

	if(!__pthread_queue) {
		errno = ESRCH;
		return 1;
	}

	pthread_context_t* ctx = (pthread_context_t*) thread;
	if(__os_thread_kill(ctx->tid, 1) == 0)
		ctx->once.done = 1;
	else {
		errno = ESRCH;
		return 1;
	}

	__pthread_counts -= 1;
	pthread_context_t* tmp = __pthread_queue;

	if(tmp->next) {
		while(tmp->next) {
			if(tmp->next == ctx) {
				tmp->next = ctx->next;
				break;
			}

			tmp = tmp->next;
		}
	} else {
		if(__pthread_queue == ctx)
			__pthread_queue = 0;
	}

	return 0;
}