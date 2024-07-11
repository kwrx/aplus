/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdatomic.h>
#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>



void sem_init(semaphore_t* s, uint32_t waiters) {

    DEBUG_ASSERT(s);
    DEBUG_ASSERT(waiters >= 0);

    spinlock_init(&s->lock);
    s->waiters = waiters;
}

#if DEBUG_LEVEL_TRACE
void __sem_wait(semaphore_t* s, const char* FUNC, const char* FILE, int LINE) {
#else
void sem_wait(semaphore_t* s) {
#endif

    DEBUG_ASSERT(s);

#if DEBUG_LEVEL_TRACE
    uint64_t t0 = arch_timer_generic_getms() + IPC_DEFAULT_TIMEOUT;
#endif

    bool wait = true;

    while (wait) {

        spinlock_lock(&s->lock);

        if (unlikely(s->waiters > 0)) {
            wait = false;
        }

        spinlock_unlock(&s->lock);

#if DEBUG_LEVEL_TRACE
        if (arch_timer_generic_getms() > t0) {
            t0 = arch_timer_generic_getms() + IPC_DEFAULT_TIMEOUT;
            kprintf("ipc: TRACE! %s(): Timeout expired for %s:%d %s(%p), cpu(%ld), tid(%d)\n", __func__, FILE, LINE, FUNC, s, current_cpu->id, current_task->tid);
        }
#endif

        __cpu_pause();
    }

    spinlock_lock(&s->lock);
    s->waiters--;
    spinlock_unlock(&s->lock);
}



int sem_trywait(semaphore_t* s) {

    DEBUG_ASSERT(s);

    spinlock_lock(&s->lock);

    int ret = 0;
    if (unlikely(s->waiters == 0)) {
        ret = 0;
    } else {
        s->waiters--;
        ret = 1;
    }

    spinlock_unlock(&s->lock);
    return ret;
}


void sem_post(semaphore_t* s) {

    DEBUG_ASSERT(s);

    spinlock_lock(&s->lock);
    s->waiters++;
    spinlock_unlock(&s->lock);
}
