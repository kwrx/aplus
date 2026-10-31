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

#include "sysfs.h"

typedef struct sysfs_root_entry {
    const char* name;
    uint8_t slot;
    unsigned char type;
} sysfs_root_entry_t;

static const sysfs_root_entry_t sysfs_root_table[] = {
    {"bus",     SYSFS_BUS_SLOT_DIR,     DT_DIR},
    {"devices", SYSFS_DEVICES_SLOT_DIR, DT_DIR},
};

static const size_t sysfs_root_entries = sizeof(sysfs_root_table) / sizeof(sysfs_root_table[0]);


ssize_t sysfs_root_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_SYSFS);
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


    __emit(SYSFS_INO_ROOT, DT_DIR, ".");
    __emit(SYSFS_INO_ROOT, DT_DIR, "..");

    for (size_t j = 0; j < sysfs_root_entries; j++) {
        __emit(SYSFS_INO_STATIC(sysfs_root_table[j].slot), sysfs_root_table[j].type, sysfs_root_table[j].name);
    }

#undef __emit

    return (ssize_t)i;
}
