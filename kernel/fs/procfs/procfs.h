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


#ifndef _PROCFS_H
#define _PROCFS_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/task.h>
#include <aplus/vfs.h>


typedef struct procfs_service {

    int (*fetch)(inode_t*, char**, size_t*, void*);

    void* arg;
    mode_t mode;

} procfs_service_t;


/* A bounded writer. Every generated /proc file is built through this rather than by
   advancing an offset with snprintf()'s return value: that idiom read `sizeof(buf) - off`
   as a size_t, so one truncating write underflowed it to a huge value and the next write
   had no bound at all. Here a full buffer simply stops accepting bytes. */
typedef struct procfs_buf {

    char* data;
    size_t capacity;
    size_t length;

} procfs_buf_t;

extern spinlock_t procfs_scratch_lock;

procfs_buf_t procfs_scratch(void);
void procfs_scratch_init(void);

void procfs_bprintf(procfs_buf_t* b, const char* fmt, ...);
void procfs_bputs(procfs_buf_t* b, const char* s, size_t len);


/* Everything a /proc file needs about a task, copied out while the run queue is locked.
   Fetches must never hold a task_t* instead: a task is freed by whoever reaps it, so a
   pointer that outlives the lock is a use-after-free -- and a task sitting in ZOMBIE has
   already had its fs, fd, sighand and address_space freed out from under it. */
typedef struct procfs_task {

    pid_t tid;
    pid_t pid;
    pid_t ppid;
    pid_t pgrp;
    pid_t sid;

    uid_t uid;
    uid_t euid;
    gid_t gid;
    gid_t egid;

    long status;
    long policy;
    long priority;

    char comm[TASK_COMM_LEN];
    char cmdline[TASK_CMDLINE_LEN];
    size_t cmdline_len;

    uint64_t start_time; /* USER_HZ ticks since boot */
    uint64_t utime;      /* USER_HZ ticks */

    uint64_t minflt;
    uint64_t majflt;
    uint64_t nvcsw;
    uint64_t nivcsw;

    uint64_t rchar;
    uint64_t wchar;
    uint64_t syscr;
    uint64_t syscw;

    size_t threads;
    cpuid_t cpu;

    /* Zero when the task has exited: its address space is gone by then. */
    size_t rss_pages;
    uintptr_t vm_start;
    uintptr_t vm_end;
    uintptr_t vm_stack;
    uintptr_t heap_start;
    uintptr_t heap_end;

    int exit_value;

} procfs_task_t;


/* One ino scheme for every producer. readdir() used to advertise hardcoded numbers that
   collided with the pids listed beside them and matched nothing getattr() later reported,
   so the same file had two different inode numbers depending on how you asked. */
#define PROCFS_INO_ROOT             ((ino_t)1)
#define PROCFS_INO_STATIC(slot)     ((ino_t)(2 + (slot)))
#define PROCFS_INO_PID(pid)         ((((ino_t)(pid)) << 8) | 0xFF)
#define PROCFS_INO_PID_FILE(pid, s) ((((ino_t)(pid)) << 8) | ((ino_t)(s)))


typedef struct procfs_root_entry {

    const char* name;
    mode_t mode;
    uint8_t slot;
    unsigned char type;

} procfs_root_entry_t;

extern const procfs_root_entry_t procfs_root_table[];
extern const size_t procfs_root_entries;


bool procfs_pid_exists(pid_t pid);
int procfs_task_snapshot(pid_t pid, procfs_task_t* out);
size_t procfs_task_list(pid_t* ids, size_t max);
char procfs_task_state(long status);

uint64_t procfs_ticks(const struct timespec* ts);


int procfs_root_getattr(inode_t* inode, struct stat* st);
inode_t* procfs_root_finddir(inode_t* inode, const char* name);
ssize_t procfs_root_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count);


inode_t* procfs_service_cmdline_inode(inode_t* parent);
inode_t* procfs_service_meminfo_inode(inode_t* parent);
inode_t* procfs_service_uptime_inode(inode_t* parent);
inode_t* procfs_service_version_inode(inode_t* parent);
inode_t* procfs_service_filesystems_inode(inode_t* parent);
inode_t* procfs_service_stat_inode(inode_t* parent);
inode_t* procfs_service_cpuinfo_inode(inode_t* parent);
inode_t* procfs_service_loadavg_inode(inode_t* parent);

inode_t* procfs_service_inode(inode_t* parent, const char* name, mode_t mode, int (*fetch)(inode_t*, char** buf, size_t*, void*), void* arg);

inode_t* procfs_service_pid_inode(inode_t* parent, pid_t pid);
inode_t* procfs_service_self_inode(inode_t* parent);
void procfs_service_pid_init(inode_t* parent);

/* Per-pid file fetches, shared with the /proc/<pid> directory table. */
int procfs_pid_fetch_stat(inode_t*, char**, size_t*, void*);
int procfs_pid_fetch_status(inode_t*, char**, size_t*, void*);
int procfs_pid_fetch_statm(inode_t*, char**, size_t*, void*);
int procfs_pid_fetch_cmdline(inode_t*, char**, size_t*, void*);
int procfs_pid_fetch_io(inode_t*, char**, size_t*, void*);

#endif
