
//
//  pthread_create.c
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


PUBLIC uint32_t __pthread_counts = 0;
PUBLIC pthread_context_t* __pthread_queue = 0;

PRIVATE void __pthread_handler__() {
	pthread_context_t* ctx = 0;
	__asm__ __volatile__ ("" : "=a"(ctx));

	if(ctx == NULL)
		_exit(-1);

	if(ctx->entry == NULL)
		_exit(-1);

	pthread_exit(ctx->entry(ctx->param));
}


PUBLIC int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void *(*start)(void*), void* arg) {
	if(!thread) {
		errno = EINVAL;
		return 1;
	}

	if(__pthread_counts > PTHREAD_THREADS_MAX) {
		errno = EAGAIN;
		return 1;
	}

	pthread_context_t* ctx = malloc(sizeof(pthread_context_t));
	memset(ctx, 0, sizeof(pthread_context_t));

	ctx->entry = start;
	ctx->param = arg;
	ctx->once.done = 0;
	ctx->once.started = -1;
	ctx->next = 0;

	if(attr)
		memcpy(&ctx->attr, attr, sizeof(pthread_attr_t));

	if(ctx->attr.param.sched_priority == 0)
		ctx->attr.param.sched_priority = TASK_PRIORITY_NORMAL;


	__pthread_counts += 1;
	*thread = (pthread_t) ctx;
	
	if(__pthread_queue) {
		pthread_context_t* tmp = __pthread_queue;
		while(tmp->next)
			tmp = tmp->next;

		tmp->next = ctx;
	} else {
		__pthread_queue = ctx;
	}


	ctx->tid = aplus_thread_create(__pthread_handler__, ctx, ctx->attr.param.sched_priority);
	return 0;
}