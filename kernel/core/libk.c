/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <aplus/utils/hashmap.h>
#include <aplus/utils/unicode.h>
#include <libc.h>


/* musl libc: 1.1.19 */


/*
 * See src/internal/locale_impl.h
 */
extern const struct __locale_struct __c_dot_utf8_locale;



/* 
 * See src/internal/pthread_impl.h 
 */
static struct {
	/* Part 1 -- these fields may be external or
	 * internal (accessed via asm) ABI. Do not change. */
	void *self;
	void **dtv, *unused1, *unused2;
	uintptr_t sysinfo;
	uintptr_t canary, canary2;
	pid_t tid, pid;

	/* Part 2 -- implementation details, non-ABI. */
	int tsd_used, errno_val;
	volatile int cancel, canceldisable, cancelasync;
	int detached;
	unsigned char *map_base;
	size_t map_size;
	void *stack;
	size_t stack_size;
	void *start_arg;
	void *(*start)(void *);
	void *result;
	struct __ptcb *cancelbuf;
	void **tsd;
	volatile int dead;
	struct {
		volatile void *volatile head;
		long off;
		volatile void *volatile pending;
	} robust_list;
	int unblock_cancel;
	volatile int timer_id;
	locale_t locale;
	volatile int killlock[1];
	volatile int exitlock[1];
	volatile int startlock[2];
	unsigned long sigmask[_NSIG/8/sizeof(long)];
	char *dlerror_buf;
	int dlerror_flag;
	void *stdio_locks;
	size_t guard_size;

	/* Part 3 -- the positions of these fields relative to
	 * the end of the structure is external and internal ABI. */
	uintptr_t canary_at_end;
	void **dtv_copy;
} kernel_pthread;


int libk_init() {
    
    kernel_pthread.self = &kernel_pthread;
    kernel_pthread.tid = 1;
    kernel_pthread.pid = 1;
    kernel_pthread.errno_val = 0;
    kernel_pthread.locale = (locale_t) &__c_dot_utf8_locale;

    int i;
    for(i = 0; i < (_NSIG / 8 / sizeof(long)); i++)
        kernel_pthread.sigmask[i] = ~0;

    return 0;
}


void* __pthread_self(void) {
    return &kernel_pthread;
}


EXPORT(__pthread_self);
EXPORT(__errno_location);