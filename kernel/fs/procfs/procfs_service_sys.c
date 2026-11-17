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
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include "procfs.h"


static int procfs_service_stat_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_buf_t b = procfs_scratch();

    uint64_t user   = 0;
    uint64_t system = 0;
    uint64_t idle   = 0;

    cpu_foreach(cpu) {
        user += procfs_ns_ticks(__atomic_load_n(&cpu->user_ns, __ATOMIC_RELAXED));
        system += procfs_ns_ticks(__atomic_load_n(&cpu->system_ns, __ATOMIC_RELAXED));
        idle += procfs_ns_ticks(__atomic_load_n(&cpu->idle_ns, __ATOMIC_RELAXED));
    }

    procfs_bprintf(&b, "cpu  %lu 0 %lu %lu 0 0 0 0 0 0\n", user, system, idle);

    cpu_foreach(cpu) {

        uint64_t us = procfs_ns_ticks(__atomic_load_n(&cpu->user_ns, __ATOMIC_RELAXED));
        uint64_t sy = procfs_ns_ticks(__atomic_load_n(&cpu->system_ns, __ATOMIC_RELAXED));
        uint64_t id = procfs_ns_ticks(__atomic_load_n(&cpu->idle_ns, __ATOMIC_RELAXED));

        procfs_bprintf(&b, "cpu%d %lu 0 %lu %lu 0 0 0 0 0 0\n", (int)cpu->id, us, sy, id);
    }

    procfs_bprintf(&b, "intr 0\n");
    procfs_bprintf(&b, "ctxt 0\n");
    procfs_bprintf(&b, "btime %lu\n", (unsigned long)(arch_timer_gettime() - (arch_timer_generic_getms() / 1000)));
    procfs_bprintf(&b, "processes %lu\n", (unsigned long)sched_nprocs());
    procfs_bprintf(&b, "procs_running %lu\n", (unsigned long)sched_nprocs());
    procfs_bprintf(&b, "procs_blocked 0\n");

    *buf  = b.data;
    *size = b.length;

    return 0;
}

inode_t* procfs_service_stat_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode      = procfs_service_inode(parent, "stat", S_IFREG | 0444, procfs_service_stat_fetch, NULL);
        inode->ino = PROCFS_INO_STATIC(7);
    }

    return inode;
}


static int procfs_service_cpuinfo_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_buf_t b = procfs_scratch();

    cpu_foreach(cpu) {

        procfs_bprintf(&b, "processor\t: %d\n", (int)cpu->id);
        procfs_bprintf(&b, "vendor_id\t: %s\n", cpu->vendor_name);
        procfs_bprintf(&b, "cpu family\t: %lu\n", (unsigned long)cpu->archid);
        procfs_bprintf(&b, "model name\t: %s\n", cpu->model_name);
        procfs_bprintf(&b, "cpu MHz\t\t: %lu\n", (unsigned long)core->cpu.max_mhz);
        procfs_bprintf(&b, "cpu cores\t: %lu\n", (unsigned long)core->cpu.max_cores);
        procfs_bprintf(&b, "siblings\t: %lu\n", (unsigned long)core->cpu.max_threads);
        procfs_bprintf(&b, "physical id\t: %lu\n", (unsigned long)cpu->node);
        procfs_bprintf(&b, "flags\t\t:\n");
        procfs_bprintf(&b, "\n");
    }

    *buf  = b.data;
    *size = b.length;

    return 0;
}

inode_t* procfs_service_cpuinfo_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode      = procfs_service_inode(parent, "cpuinfo", S_IFREG | 0444, procfs_service_cpuinfo_fetch, NULL);
        inode->ino = PROCFS_INO_STATIC(8);
    }

    return inode;
}


static int procfs_service_loadavg_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_buf_t b = procfs_scratch();

    size_t n = sched_nprocs();

    procfs_bprintf(&b, "0.00 0.00 0.00 %lu/%lu %d\n", (unsigned long)n, (unsigned long)n, (int)sched_lastpid());

    *buf  = b.data;
    *size = b.length;

    return 0;
}

inode_t* procfs_service_loadavg_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode      = procfs_service_inode(parent, "loadavg", S_IFREG | 0444, procfs_service_loadavg_fetch, NULL);
        inode->ino = PROCFS_INO_STATIC(9);
    }

    return inode;
}


static int procfs_service_self_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_buf_t b = procfs_scratch();

    procfs_bprintf(&b, "%d", current_task->pid);

    *buf  = b.data;
    *size = b.length;

    return 0;
}

inode_t* procfs_service_self_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode      = procfs_service_inode(parent, "self", S_IFLNK | 0777, procfs_service_self_fetch, NULL);
        inode->ino = PROCFS_INO_STATIC(1);
    }

    return inode;
}


static int procfs_service_mounts_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_buf_t b = procfs_scratch();

    procfs_bprintf(&b, "self/mounts");

    *buf  = b.data;
    *size = b.length;

    return 0;
}

inode_t* procfs_service_mounts_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode      = procfs_service_inode(parent, "mounts", S_IFLNK | 0777, procfs_service_mounts_fetch, NULL);
        inode->ino = PROCFS_INO_STATIC(10);
    }

    return inode;
}
