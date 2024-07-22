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
#include <aplus/vfs.h>


typedef struct procfs_service {

    int (*fetch)(inode_t*, char**, size_t*, void*);

    void* arg;
    mode_t mode;

} procfs_service_t;



inode_t* procfs_root_finddir(inode_t* inode, const char* name);
ssize_t procfs_root_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count);


inode_t* procfs_service_cmdline_inode(inode_t* parent, pid_t pid);
inode_t* procfs_service_meminfo_inode(inode_t* parent);
inode_t* procfs_service_uptime_inode(inode_t* parent);

inode_t* procfs_service_inode(inode_t* parent, char* name, mode_t mode, int (*fetch)(inode_t*, char** buf, size_t*, void*), void* arg);

task_t* procfs_service_pid_to_task(pid_t id);
inode_t* procfs_service_pid_inode(inode_t* parent, pid_t pid);
void procfs_service_pid_init(inode_t* parent);

#endif
