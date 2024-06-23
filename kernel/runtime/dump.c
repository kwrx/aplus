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

#include <limits.h>
#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/elf.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/utils/ptr.h>


static void print_path(const char* prefix, inode_t* inode) {

    char* ppath[CONFIG_PATH_MAX] = {0};

    size_t i = 0;

    for (inode_t* tmp = inode; tmp; tmp = tmp->parent) {
        ppath[i++] = tmp->name;
    }

    kprintf("%s", prefix);

    while (i > 0) {
        kprintf("/%s", ppath[--i]);
    }

    kprintf("\n");
}

void runtime_dump() {

    kprintf("--- Process Info for cpu(%ld), pid(%d) ---\n", current_cpu->id, current_task ? current_task->tid : -1);

    if (likely(current_task)) {

        shared_ptr_nullable_access(current_task->fs, fs, {
            print_path(" Executable: ", fs->exe);
            print_path(" Current Directory: ", fs->cwd);
            print_path(" Root: ", fs->root);
        });

        kprintf(" Thread ID: %d\n", current_task->tid);
        kprintf(" Process ID: %d\n", current_task->pid);
        kprintf(" Parent Process ID: %d\n", current_task->parent ? current_task->parent->pid : -1);
        kprintf(" Process Group ID: %d\n", current_task->pgrp);
        kprintf(" Session ID: %d\n", current_task->sid);
        kprintf(" User ID: %d\n", current_task->uid);
        kprintf(" Group ID: %d\n", current_task->gid);
        kprintf(" Effective User ID: %d\n", current_task->euid);
        kprintf(" Effective Group ID: %d\n", current_task->egid);
        kprintf(" Address Space PM: 0x%8lX\n", current_task->address_space ? current_task->address_space->pm : 0);

    } else {

        kprintf(" No task is currently running\n");
    }

    kprintf("--- End of Process Info ---\n");
}
