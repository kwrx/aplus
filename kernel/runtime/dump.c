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

//? A panic printer that faults is worse than no panic printer: the second fault re-enters
//? this path and the machine spins printing the same line forever instead of the one that
//? matters. Anything reached through current_task is checked before it is followed, because
//? the reason this is running at all may well be that one of those pointers is rubbish.
static bool plausible(const void* p) {

    //? A range test rather than a page table walk: the walk takes the address space lock,
    //? which whoever panicked may well be holding, and every task_t here comes from
    //? kmalloc, which only ever hands out addresses in the heap area.
    return (uintptr_t)p >= arch_vmm_p2v(0, ARCH_VMM_AREA_HEAP);
}


void runtime_dump() {

    kprintf("--- Process Info for cpu(%ld), pid(%d) ---\n", current_cpu->id, current_task ? current_task->tid : -1);

    kprintf(" Task: %p\n", current_task);

    if (unlikely(current_task && !plausible(current_task))) {

        kprintf(" Task pointer is not mapped; nothing else here can be trusted\n");
        kprintf("--- End of Process Info ---\n");

        return;
    }

    if (likely(current_task)) {

        shared_ptr_nullable_access(current_task->fs, fs, {
            print_path(" Executable: ", fs->exe);
            print_path(" Current Directory: ", fs->cwd);
            print_path(" Root: ", fs->root);
        });

        kprintf(" Thread ID: %d\n", current_task->tid);
        kprintf(" Process ID: %d\n", current_task->pid);
        kprintf(" Parent: %p\n", current_task->parent);
        kprintf(" Parent Process ID: %d\n", plausible(current_task->parent) ? current_task->parent->pid : -1);
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
