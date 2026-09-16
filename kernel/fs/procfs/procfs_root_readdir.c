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



#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include "procfs.h"


/**
 * @brief The contents of /proc other than the per-pid directories, driving both finddir() and readdir().
 */
const procfs_root_entry_t procfs_root_table[] = {
    {"self",        S_IFLNK | 0777, 1, DT_LNK},
    {"meminfo",     S_IFREG | 0444, 2, DT_REG},
    {"uptime",      S_IFREG | 0444, 3, DT_REG},
    {"version",     S_IFREG | 0444, 4, DT_REG},
    {"filesystems", S_IFREG | 0444, 5, DT_REG},
    {"cmdline",     S_IFREG | 0444, 6, DT_REG},
    {"stat",        S_IFREG | 0444, 7, DT_REG},
    {"cpuinfo",     S_IFREG | 0444, 8, DT_REG},
    {"loadavg",     S_IFREG | 0444, 9, DT_REG},
};

const size_t procfs_root_entries = sizeof(procfs_root_table) / sizeof(procfs_root_table[0]);


/**
 * @brief Lists /proc, the fixed entries first and the pids after them.
 *
 * @param inode The /proc root inode.
 * @param e The array of dirents to fill in.
 * @param pos The ordinal of the first entry to report.
 * @param count The number of entries the caller's buffer holds.
 * @return The number of entries written, 0 past the end of the listing, or -1 with errno set.
 */
ssize_t procfs_root_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->sb->root == inode);
    DEBUG_ASSERT(e);

    if (unlikely(count == 0))
        return 0;

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;


    size_t i = 0;

#define __emit(_ino, _type, _name)                          \
    do {                                                    \
                                                            \
        if (pos-- > 0)                                      \
            break;                                          \
                                                            \
        e[i].d_ino  = (_ino);                               \
        e[i].d_off  = (off_t)(i);                           \
        e[i].d_type = (_type);                              \
                                                            \
        strncpy(e[i].d_name, (_name), sizeof(e[i].d_name)); \
        e[i].d_name[sizeof(e[i].d_name) - 1] = '\0';        \
                                                            \
        e[i].d_reclen = sizeof(struct dirent);              \
                                                            \
        if (++i == count)                                   \
            return (ssize_t)i;                              \
                                                            \
    } while (0)


    __emit(PROCFS_INO_ROOT, DT_DIR, ".");
    __emit(PROCFS_INO_ROOT, DT_DIR, "..");

    for (size_t j = 0; j < procfs_root_entries; j++) {
        __emit(PROCFS_INO_STATIC(procfs_root_table[j].slot), procfs_root_table[j].type, procfs_root_table[j].name);
    }


    if (pos < 0)
        return (ssize_t)i;


    size_t max = sched_nprocs() + 32;

    pid_t* ids = (pid_t*)kcalloc(max, sizeof(pid_t), GFP_KERNEL);

    if (unlikely(!ids))
        return (ssize_t)i;

    size_t n = procfs_task_list(ids, max);


    for (size_t j = 0; j < n; j++) {

        if (pos-- > 0)
            continue;

        e[i].d_ino  = PROCFS_INO_PID(ids[j]);
        e[i].d_off  = (off_t)(i);
        e[i].d_type = DT_DIR;

        snprintf(e[i].d_name, sizeof(e[i].d_name), "%d", ids[j]);

        e[i].d_reclen = sizeof(struct dirent);

        if (++i == count)
            break;
    }

#undef __emit

    kfree(ids);

    return (ssize_t)i;
}
