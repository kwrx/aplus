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
#include <sys/mount.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "procfs.h"


ssize_t procfs_root_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->sb->root == inode);

    DEBUG_ASSERT(e);

    if (unlikely(count == 0))
        return 0;


    int i = 0;

    cpu_foreach(cpu) {

        task_t* task = cpu->sched_queue;

        for (task_t* q = cpu->sched_queue; q; q = q->next) {

            if (pos-- > 0)
                continue;


            e[i].d_ino    = task->tid;
            e[i].d_off    = pos;
            e[i].d_reclen = sizeof(struct dirent);
            e[i].d_type   = DT_DIR;

            snprintf(e->d_name, sizeof(e->d_name), "%d", task->tid);

            if (++i == count)
                return i;
        }
    }


#define __readdir(ino, type, name)                       \
    do {                                                 \
                                                         \
        if (pos-- > 0)                                   \
            break;                                       \
                                                         \
        e[i].d_ino    = ino;                             \
        e[i].d_off    = pos;                             \
        e[i].d_reclen = sizeof(struct dirent);           \
        e[i].d_type   = type;                            \
                                                         \
        strncpy(e[i].d_name, name, sizeof(e[i].d_name)); \
                                                         \
        if (++i == count)                                \
            return i;                                    \
                                                         \
    } while (0)


    __readdir(1, DT_DIR, ".");
    __readdir(2, DT_DIR, "..");
    __readdir(3, DT_DIR, "self");
    __readdir(4, DT_REG, "cpuinfo");
    __readdir(4, DT_REG, "meminfo");
    __readdir(5, DT_REG, "uptime");
    __readdir(6, DT_REG, "version");
    __readdir(7, DT_REG, "modules");
    __readdir(8, DT_REG, "mounts");
    __readdir(9, DT_REG, "filesystems");
    __readdir(10, DT_REG, "cmdline");

    return i;
}
