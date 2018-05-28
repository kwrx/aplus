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


#ifndef _PROCFS_H
#define _PROCFS_H

#include <aplus.h>
#include <aplus/base.h>
#include <aplus/task.h>
#include <aplus/debug.h>
#include <aplus/utils/list.h>

#define PROCFS_DATA_SIZE                                        8192

#define PROCFS_ENTRY(name)                                      \
    extern int procfs_##name##_init(procfs_entry_t*);           \
    extern int procfs_##name##_update(procfs_entry_t*)

#define procfs_mkentry(x, y, z)                                 \
    __procfs_mkentry(x, y, #z, procfs_##z##_init, procfs_##z##_update, NULL)

#define procfs_mkentry_with_arg(x, y, z, w)                     \
    __procfs_mkentry(x, y, #z, procfs_##z##_init, procfs_##z##_update, (void*) w)


typedef struct procfs_entry {
    char name[64];
    char data[PROCFS_DATA_SIZE];
    volatile task_t* task;
    
    size_t size;
    mode_t mode;
    void* arg;
    void* link;


    int (*init) (struct procfs_entry*);
    int (*update) (struct procfs_entry*);

    struct procfs_entry* parent;
    list(struct procfs_entry*, childs);
} procfs_entry_t;


PROCFS_ENTRY(cmdline);
PROCFS_ENTRY(cwd);
PROCFS_ENTRY(devices);
PROCFS_ENTRY(environ);
PROCFS_ENTRY(exe);
PROCFS_ENTRY(fd);
PROCFS_ENTRY(fdnode);
PROCFS_ENTRY(filesystems);
PROCFS_ENTRY(io);
PROCFS_ENTRY(meminfo);
PROCFS_ENTRY(modules);
PROCFS_ENTRY(mountinfo);
PROCFS_ENTRY(mounts);
PROCFS_ENTRY(mountstats);
PROCFS_ENTRY(pid);
PROCFS_ENTRY(root);
PROCFS_ENTRY(stat);
PROCFS_ENTRY(statm);
PROCFS_ENTRY(status);
PROCFS_ENTRY(uptime);
PROCFS_ENTRY(version);



procfs_entry_t* __procfs_mkentry(procfs_entry_t* parent, volatile task_t* task, char* name,
                                    int (*init) (procfs_entry_t*), int (*update) (procfs_entry_t*), void* arg);

int procfs_mount(struct inode* dev, struct inode* dir, struct mountinfo* info);
int procfs_read(struct inode* inode, void* ptr, off_t pos, size_t len);
int procfs_open(struct inode* inode);
struct inode* procfs_finddir(struct inode* inode, char* name);

int procfs_update(struct procfs_entry*);
int procfs_init(struct procfs_entry*);

#endif
