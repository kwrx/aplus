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


#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <libc.h>

SYSCALL(128, init_module,
int sys_init_module(void* image, unsigned long len, const char* param_values) {
    if(unlikely(!image || !len)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(current_task->uid != TASK_ROOT_UID)) {
        errno = EPERM;
        return -1;
    }

    char* name;
    if(module_check(image, (size_t) len, &name) != 0)
        return -1;

    if(module_load(name) != 0)
        return -1;

    if(module_run(name) != 0)
        return -1;

    return 0;
});