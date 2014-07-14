
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

PUBLIC int pthread_join(pthread_t thread, void **value_ptr) {
	if(!thread) {
		errno = EINVAL;
		return 1;
	}

	pthread_context_t* ctx = (pthread_context_t*) thread;
	
	__os_thread_idle();
	while(ctx->once.done == 0) {
		#if ARCH==X86
			__asm__ __volatile__ ("pause");
		#elif ARCH==X86_64
			__asm__ __volatile__ ("pause");
		#else
			/* Nothing */
		#endif
	}
	__os_thread_wakeup();

	if(value_ptr)
		*value_ptr = ctx->exitval;

	return 0;
}