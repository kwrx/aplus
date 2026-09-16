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



#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include "procfs.h"


static int __snapshot_of(void* arg, procfs_task_t* t) {

    pid_t pid = (pid_t)((uintptr_t)arg);

    return procfs_task_snapshot(pid, t);
}


/**
 * @brief Generates /proc/<pid>/stat, the 52 space-separated fields ps parses.
 *
 * @param inode The /proc/<pid>/stat inode.
 * @param buf Receives the generated contents.
 * @param size Receives the length of the contents.
 * @param arg Unused.
 * @return 0 on success, or -1 with errno set.
 */
int procfs_pid_fetch_stat(inode_t* inode, char** buf, size_t* size, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_task_t t;

    if (__snapshot_of(arg, &t) < 0)
        return errno = ESRCH, -1;


    procfs_buf_t b = procfs_scratch();

    procfs_bprintf(&b, "%d (%s) %c %d %d %d ", t.tid, t.comm, procfs_task_state(t.status), t.ppid, t.pgrp, t.sid);

    procfs_bprintf(&b, "0 -1 0 ");

    procfs_bprintf(&b, "%lu 0 %lu 0 ", t.minflt, t.majflt);

    procfs_bprintf(&b, "%lu 0 0 0 ", t.utime);

    procfs_bprintf(&b, "%ld %ld %lu 0 %lu ", t.priority, t.priority, (unsigned long)t.threads, t.start_time);

    procfs_bprintf(&b, "%lu %lu ", (unsigned long)(t.vm_end - t.vm_start), (unsigned long)t.rss_pages);

    procfs_bprintf(&b, "0 %lu %lu %lu ", (unsigned long)t.vm_start, (unsigned long)t.vm_end, (unsigned long)t.vm_stack);

    procfs_bprintf(&b, "0 0 0 0 0 0 ");

    procfs_bprintf(&b, "0 0 0 17 ");

    procfs_bprintf(&b, "%d 0 %ld ", (int)t.cpu, t.policy);

    procfs_bprintf(&b, "0 0 0 0 0 0 0 0 0 0 ");

    procfs_bprintf(&b, "%d\n", t.exit_value);


    *buf  = b.data;
    *size = b.length;

    return 0;
}


int procfs_pid_fetch_status(inode_t* inode, char** buf, size_t* size, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_task_t t;

    if (__snapshot_of(arg, &t) < 0)
        return errno = ESRCH, -1;


    procfs_buf_t b = procfs_scratch();

    static const char* const states[] = {"running", "running", "sleeping", "stopped", "zombie", "dead"};

    const char* state = (t.status >= 0 && t.status <= TASK_STATUS_DEAD) ? states[t.status] : "unknown";

    procfs_bprintf(&b, "Name:\t%s\n", t.comm);
    procfs_bprintf(&b, "State:\t%c (%s)\n", procfs_task_state(t.status), state);
    procfs_bprintf(&b, "Tgid:\t%d\n", t.pid);
    procfs_bprintf(&b, "Pid:\t%d\n", t.tid);
    procfs_bprintf(&b, "PPid:\t%d\n", t.ppid);
    procfs_bprintf(&b, "TracerPid:\t0\n");
    procfs_bprintf(&b, "Uid:\t%d\t%d\t%d\t%d\n", t.uid, t.euid, t.euid, t.euid);
    procfs_bprintf(&b, "Gid:\t%d\t%d\t%d\t%d\n", t.gid, t.egid, t.egid, t.egid);
    procfs_bprintf(&b, "FDSize:\t%d\n", CONFIG_OPEN_MAX);
    procfs_bprintf(&b, "Groups:\t\n");
    procfs_bprintf(&b, "VmSize:\t%lu kB\n", (unsigned long)((t.vm_end - t.vm_start) >> 10));
    procfs_bprintf(&b, "VmRSS:\t%lu kB\n", (unsigned long)((t.rss_pages * PML1_PAGESIZE) >> 10));
    procfs_bprintf(&b, "Threads:\t%lu\n", (unsigned long)t.threads);
    procfs_bprintf(&b, "voluntary_ctxt_switches:\t%lu\n", t.nvcsw);
    procfs_bprintf(&b, "nonvoluntary_ctxt_switches:\t%lu\n", t.nivcsw);

    *buf  = b.data;
    *size = b.length;

    return 0;
}


int procfs_pid_fetch_statm(inode_t* inode, char** buf, size_t* size, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_task_t t;

    if (__snapshot_of(arg, &t) < 0)
        return errno = ESRCH, -1;


    procfs_buf_t b = procfs_scratch();

    unsigned long size_pages = (unsigned long)((t.vm_end - t.vm_start) / PML1_PAGESIZE);
    unsigned long data_pages = (unsigned long)((t.heap_end - t.heap_start) / PML1_PAGESIZE);

    procfs_bprintf(&b, "%lu %lu 0 0 0 %lu 0\n", size_pages, (unsigned long)t.rss_pages, data_pages);

    *buf  = b.data;
    *size = b.length;

    return 0;
}


/**
 * @brief Generates /proc/<pid>/cmdline, argv NUL-separated and empty for a kernel thread.
 *
 * @param inode The /proc/<pid>/cmdline inode.
 * @param buf Receives the generated contents.
 * @param size Receives the length of the contents.
 * @param arg Unused.
 * @return 0 on success, or -1 with errno set.
 */
int procfs_pid_fetch_cmdline(inode_t* inode, char** buf, size_t* size, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_task_t t;

    if (__snapshot_of(arg, &t) < 0)
        return errno = ESRCH, -1;


    procfs_buf_t b = procfs_scratch();

    procfs_bputs(&b, t.cmdline, t.cmdline_len);

    *buf  = b.data;
    *size = b.length;

    return 0;
}


int procfs_pid_fetch_io(inode_t* inode, char** buf, size_t* size, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_task_t t;

    if (__snapshot_of(arg, &t) < 0)
        return errno = ESRCH, -1;


    procfs_buf_t b = procfs_scratch();

    procfs_bprintf(&b, "rchar: %lu\n", t.rchar);
    procfs_bprintf(&b, "wchar: %lu\n", t.wchar);
    procfs_bprintf(&b, "syscr: %lu\n", t.syscr);
    procfs_bprintf(&b, "syscw: %lu\n", t.syscw);

    *buf  = b.data;
    *size = b.length;

    return 0;
}
