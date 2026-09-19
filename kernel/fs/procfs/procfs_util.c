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


/**
 * @brief The one buffer every generated /proc file is written into, and the lock that guards it.
 */
static char procfs_scratch_buffer[8192];

spinlock_t procfs_scratch_lock = {0};


/**
 * @brief Prepares the scratch buffer's lock.
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
 * @brief Copies out everything /proc reports about a task, with the run queue held.
 *
 * @param t The task to read.
 * @param cpu The cpu the task sits on.
 * @param threads The number of threads in its group.
 * @param o Receives the snapshot.
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
 * @brief Snapshots the live thread-group leaders into a caller-provided array.
 *
 * @param ids Receives the pids.
 * @param max The number of pids the array holds.
 * @return The number of pids written.
 */
size_t procfs_task_list(pid_t* ids, size_t max) {

    DEBUG_ASSERT(ids);

    size_t n = 0;

    cpu_foreach (cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t && n < max; t = t->next) {

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


/**
 * @brief Writes the absolute path an inode sits at, walking up its parent chain.
 *
 * @param inode The inode to name.
 * @param root The root the path is written relative to, as the owning task sees it.
 * @param buf Receives the path, NUL terminated.
 * @param size The size of the buffer.
 * @return The length of the path, or -1 with errno set.
 */
static ssize_t __inode_path(inode_t* inode, inode_t* root, char* buf, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);

    if (unlikely(size < 2))
        return errno = ENAMETOOLONG, -1;


    if (inode->flags & INODE_FLAGS_ANONYMOUS)
        return (ssize_t)snprintf(buf, size, "anon_inode:[%lu]", (unsigned long)inode->ino);


    size_t pos = size - 1;

    buf[pos] = '\0';

    for (inode_t* i = inode; i != root && i->parent; i = i->parent) {

        size_t len = strlen(i->name);

        if (unlikely(len + 1 > pos))
            return errno = ENAMETOOLONG, -1;

        pos -= len;
        memcpy(&buf[pos], i->name, len);

        buf[--pos] = '/';
    }

    if (pos == size - 1)
        buf[--pos] = '/';

    memmove(buf, &buf[pos], size - pos);

    return (ssize_t)(size - pos - 1);
}


bool procfs_task_fd_exists(pid_t pid, int fd) {

    if (unlikely(pid <= 0))
        return false;

    if (unlikely(fd < 0 || fd >= CONFIG_OPEN_MAX))
        return false;


    cpu_foreach (cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t; t = t->next) {

                if (t->tid != pid || t->status == TASK_STATUS_DEAD)
                    continue;


                bool open = false;

                shared_ptr_access(t->fd, fds, {
                    open = fds->descriptors[fd].ref != NULL;
                });

                return open;
            }
        }
    }

    return false;
}


/**
 * @brief Snapshots the descriptors a task currently holds open.
 *
 * @param pid The tid of the task to inspect.
 * @param out Receives the descriptor numbers.
 * @param max The number of descriptors the array holds.
 * @return The number of descriptors written.
 */
size_t procfs_task_fd_list(pid_t pid, int* out, size_t max) {

    DEBUG_ASSERT(out);

    if (unlikely(pid <= 0))
        return 0;


    cpu_foreach (cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t; t = t->next) {

                if (t->tid != pid || t->status == TASK_STATUS_DEAD)
                    continue;


                size_t n = 0;

                shared_ptr_access(t->fd, fds, {
                    for (int i = 0; i < CONFIG_OPEN_MAX && n < max; i++) {

                        if (fds->descriptors[i].ref == NULL)
                            continue;

                        out[n++] = i;
                    }
                });

                return n;
            }
        }
    }

    return 0;
}


ssize_t procfs_task_fd_path(pid_t pid, int fd, char* buf, size_t size) {

    DEBUG_ASSERT(buf);

    if (unlikely(pid <= 0))
        return errno = ESRCH, -1;

    if (unlikely(fd < 0 || fd >= CONFIG_OPEN_MAX))
        return errno = EBADF, -1;


    cpu_foreach (cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t; t = t->next) {

                if (t->tid != pid || t->status == TASK_STATUS_DEAD)
                    continue;


                inode_t* root = NULL;

                shared_ptr_nullable_access(t->fs, fs, {
                    root = fs->root;
                });


                ssize_t e = (errno = EBADF, -1);

                shared_ptr_access(t->fd, fds, {
                    if (fds->descriptors[fd].ref && fds->descriptors[fd].ref->inode)
                        e = __inode_path(fds->descriptors[fd].ref->inode, root, buf, size);
                });

                return e;
            }
        }
    }

    return errno = ESRCH, -1;
}
