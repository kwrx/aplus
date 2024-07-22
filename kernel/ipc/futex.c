/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdbool.h>
#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>



static spinlock_t rt_lock = SPINLOCK_INIT;


void futex_rt_lock() {
    spinlock_lock(&rt_lock);
}

void futex_rt_unlock() {
    spinlock_unlock(&rt_lock);
}

#if DEBUG_LEVEL_TRACE
void __futex_wait(task_t* task, volatile uint32_t* kaddr, uint32_t value, const struct timespec* utime, const char* OBJ, const char* FILE, int LINE) {
#else
void futex_wait(task_t* task, volatile uint32_t* kaddr, uint32_t value, const struct timespec* utime) {
#endif

    DEBUG_ASSERT(task);
    DEBUG_ASSERT(kaddr);

#if DEBUG_LEVEL_TRACE
    // // kprintf("futex: futex_wait() for '%s' at %s:%d - pid(%d) kaddr(%p) *kaddr(%d) value(%d) timeout(%p)\n", OBJ, FILE, LINE, task->tid, kaddr, *kaddr, value, utime);
#endif



    list_each(task->futexes, futex) {

        if (likely(futex->address != kaddr))
            continue;


        __lock(&task->lock, {
            futex->address = kaddr;
            futex->value   = value;

            if (utime) {

                memcpy(&futex->timeout, utime, sizeof(struct timespec));

                futex->timeout.tv_sec += arch_timer_generic_getms() / 1000ULL;
                futex->timeout.tv_nsec += arch_timer_generic_getns() % 1000000000ULL;

            } else {

                futex->timeout.tv_sec  = 0;
                futex->timeout.tv_nsec = 0;
            }
        });

        return;
    }



    futex_t* futex = (futex_t*)kcalloc(1, sizeof(futex_t), GFP_KERNEL);

    futex->address = kaddr;
    futex->value   = value;

    if (utime) {

        memcpy(&futex->timeout, utime, sizeof(struct timespec));

        futex->timeout.tv_sec += arch_timer_generic_getms() / 1000ULL;
        futex->timeout.tv_nsec += arch_timer_generic_getns() % 1000000000ULL;
    }


    __lock(&task->lock, { list_push(task->futexes, futex); });
}


size_t futex_wakeup(uint32_t* kaddr, size_t max) {

    DEBUG_ASSERT(kaddr);

#if DEBUG_LEVEL_TRACE
    kprintf("futex: futex_wakeup() pid(%d) kaddr(%p) *kaddr(%d), max(%ld)\n", current_task->tid, kaddr, *kaddr, max);
#endif


    size_t wok = 0;


    cpu_foreach(cpu) {

        task_t* tmp;
        for (tmp = cpu->sched_queue; tmp && max; tmp = tmp->next) {

            __lock(&tmp->lock, {

                list_each(tmp->futexes, i) {

                    if(likely(i->address != kaddr))
                        continue;


#if DEBUG_LEVEL_TRACE
                    kprintf("futex: woke up pid(%d) kaddr(%p)\n", tmp->tid, kaddr);
#endif


                    i->address =  0;
                    i->value   = ~0;

                    max--; wok++;
                    break;
        }
    });
}
}

return wok;
}


size_t futex_requeue(uint32_t* kaddr, uint32_t* kaddr2, size_t max) {

    DEBUG_ASSERT(kaddr);
    DEBUG_ASSERT(kaddr2);

#if DEBUG_LEVEL_TRACE
    kprintf("futex: futex_requeue() pid(%d) kaddr(%p) kaddr2(%p) max(%ld)\n", current_task->tid, kaddr, kaddr2, max);
#endif


    size_t req = 0;


    cpu_foreach(cpu) {

        task_t* tmp;
        for (tmp = cpu->sched_queue; tmp && max; tmp = tmp->next) {

            __lock(&tmp->lock, {

                list_each(tmp->futexes, i) {

                    if(likely(i->address != kaddr))
                        continue;


#if DEBUG_LEVEL_TRACE
                    kprintf("futex: requeue pid(%d) from kaddr(%p) to kaddr2(%p)\n", tmp->tid, kaddr, kaddr2);
#endif

                    i->address = kaddr2;

                    max--; req++;
                    break;
        }
    });
}
}

return req;
}



bool futex_expired(futex_t* futex) {

    DEBUG_ASSERT(futex);

    if (futex->address == NULL)
        return true;

    if (atomic_load(futex->address) != futex->value)
        return true;

    if (futex->timeout.tv_sec + futex->timeout.tv_nsec == 0)
        return false;

    return (arch_timer_generic_getns() > (futex->timeout.tv_sec * 1000000000ULL + futex->timeout.tv_nsec));
}
