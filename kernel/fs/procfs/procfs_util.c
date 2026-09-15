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



#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include "procfs.h"


/* One buffer, shared by every generated /proc file. It is not per-inode because a per-pid
   file has one inode per pid, so N readers of N pids would otherwise be writing the same
   static array at once; and it is not per-read because allocating on the read path would
   mean calling the heap from inside the VFS. procfs_service_read() holds this lock across
   both the fetch and the copy out, so nothing can overwrite the contents in between. */
static char procfs_scratch_buffer[8192];

spinlock_t procfs_scratch_lock = {0};


/**
 * @brief Prepare the scratch buffer's lock.
 *
 * Required: a zeroed spinlock_t reads as owned by task 0 rather than free -- spinlock_init()
 * sets owner to -1ULL -- so the first acquisition of a statically-zeroed lock fails.
 */
void procfs_scratch_init(void) {
    spinlock_init(&procfs_scratch_lock);
}


procfs_buf_t procfs_scratch(void) {

    return (procfs_buf_t){.data = procfs_scratch_buffer, .capacity = sizeof(procfs_scratch_buffer), .length = 0};
}


void procfs_bputs(procfs_buf_t* b, const char* s, size_t len) {

    DEBUG_ASSERT(b);
    DEBUG_ASSERT(b->data);

    if (unlikely(b->length >= b->capacity))
        return;

    size_t room = b->capacity - b->length;

    if (len > room)
        len = room;

    memcpy(&b->data[b->length], s, len);

    b->length += len;
}


void procfs_bprintf(procfs_buf_t* b, const char* fmt, ...) {

    DEBUG_ASSERT(b);
    DEBUG_ASSERT(b->data);

    if (unlikely(b->length >= b->capacity))
        return;


    va_list v;
    va_start(v, fmt);

    //? vsnprintf() reports the length the output would have had, so a truncated write is
    //? visible here rather than silently advancing the cursor past the end.
    int n = vsnprintf(&b->data[b->length], b->capacity - b->length, fmt, v);

    va_end(v);


    if (unlikely(n < 0))
        return;

    if ((size_t)n >= b->capacity - b->length)
        b->length = b->capacity;
    else
        b->length += (size_t)n;
}


uint64_t procfs_ticks(const struct timespec* ts) {

    DEBUG_ASSERT(ts);

    //? Both halves are signed; a negative one would wrap to something enormous once cast,
    //? which is how a broken carry in the scheduler surfaced here as a 1.8e12-tick utime.
    if (unlikely(ts->tv_sec < 0))
        return 0;

    uint64_t ticks = (uint64_t)ts->tv_sec * TASK_USER_HZ;

    if (likely(ts->tv_nsec > 0))
        ticks += (uint64_t)ts->tv_nsec / (1000000000UL / TASK_USER_HZ);

    return ticks;
}


char procfs_task_state(long status) {

    switch (status) {

        case TASK_STATUS_READY:
        case TASK_STATUS_RUNNING:
            return 'R';
        case TASK_STATUS_SLEEP:
            return 'S';
        case TASK_STATUS_STOP:
            return 'T';
        case TASK_STATUS_ZOMBIE:
            return 'Z';
        default:
            return 'X';
    }
}


/**
 * @brief Copy out everything /proc reports about a task, with the run queue held.
 *
 * Holding cpu->sched_lock is what keeps the task alive: sched_dequeue() unlinks and
 * destroys a task under that same lock, so anything read without it races a reaper. The
 * copy is a plain struct so no pointer escapes the critical section.
 *
 * A task that has exited keeps its slot on the queue until it is reaped, but sys_exit()
 * has already freed its address space -- so the memory figures are only read when the
 * task is still alive to have them.
 */
static void __snapshot(task_t* t, cpuid_t cpu, size_t threads, procfs_task_t* o) {

    memset(o, 0, sizeof(procfs_task_t));

    o->tid  = t->tid;
    o->pid  = (pid_t)t->pid;
    o->ppid = t->ppid;
    o->pgrp = t->pgrp;
    o->sid  = (pid_t)t->sid;

    o->uid  = t->uid;
    o->euid = t->euid;
    o->gid  = t->gid;
    o->egid = t->egid;

    o->status   = (long)t->status;
    o->policy   = (long)t->policy;
    o->priority = (long)t->priority;

    memcpy(o->comm, t->comm, TASK_COMM_LEN);
    memcpy(o->cmdline, t->cmdline, TASK_CMDLINE_LEN);

    o->cmdline_len = t->cmdline_len;

    if (o->cmdline_len > TASK_CMDLINE_LEN)
        o->cmdline_len = TASK_CMDLINE_LEN;

    o->start_time = t->start_time;
    o->utime      = procfs_ticks(&t->clock[TASK_CLOCK_THREAD_CPUTIME]);

    o->minflt = t->rusage.ru_minflt;
    o->majflt = t->rusage.ru_majflt;
    o->nvcsw  = t->rusage.ru_nvcsw;
    o->nivcsw = t->rusage.ru_nivcsw;

    o->rchar = t->iostat.rchar;
    o->wchar = t->iostat.wchar;
    o->syscr = t->iostat.syscr;
    o->syscw = t->iostat.syscw;

    o->threads = threads;
    o->cpu     = cpu;

    o->exit_value = t->exit.value;

    o->vm_start = t->userspace.start;
    o->vm_end   = t->userspace.end;
    o->vm_stack = t->userspace.stack;

    //? Cleared by sys_exit(); a zombie has no address space left to describe.
    if (likely(t->address_space)) {

        o->rss_pages  = t->address_space->size;
        o->heap_start = t->address_space->mmap.heap_start;
        o->heap_end   = t->address_space->mmap.heap_end;
    }
}


int procfs_task_snapshot(pid_t pid, procfs_task_t* out) {

    DEBUG_ASSERT(out);

    if (unlikely(pid <= 0))
        return errno = ESRCH, -1;


    //? Threads of a process share its tgid, and /proc/<pid>/stat reports how many there
    //? are, so the queues are counted in the same pass that finds the task.
    size_t threads = 0;
    bool found     = false;

    cpu_foreach (cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t; t = t->next) {

                if ((pid_t)t->pid == pid)
                    threads++;

                if (t->tid != pid)
                    continue;

                if (t->status == TASK_STATUS_DEAD)
                    continue;

                __snapshot(t, cpu->id, 0, out);

                found = true;
            }
        }
    }


    if (!found)
        return errno = ESRCH, -1;

    out->threads = threads;

    return 0;
}


bool procfs_pid_exists(pid_t pid) {

    if (unlikely(pid <= 0))
        return false;


    cpu_foreach (cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t; t = t->next) {

                if (t->tid == pid && t->status != TASK_STATUS_DEAD)
                    return true;
            }
        }
    }

    return false;
}


/**
 * @brief Snapshot the live thread-group leaders into a caller-provided array.
 *
 * Taken in one pass so a listing cannot skip or repeat an entry: readdir() is called once
 * per directory entry with an ordinal, and re-walking a live run queue for each of those
 * meant a fork or exit between two calls silently shifted every later position.
 *
 * The array is allocated by the caller, because allocating here would mean calling into
 * the heap with a spinlock held and interrupts disabled.
 */
size_t procfs_task_list(pid_t* ids, size_t max) {

    DEBUG_ASSERT(ids);

    size_t n = 0;

    cpu_foreach (cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t && n < max; t = t->next) {

                //? Only thread-group leaders get a /proc/<pid> directory, as on Linux --
                //? otherwise every thread of a process shows up as its own process.
                if (t->tid != (pid_t)t->pid)
                    continue;

                if (t->status == TASK_STATUS_DEAD)
                    continue;

                ids[n++] = t->tid;
            }
        }
    }

    return n;
}
