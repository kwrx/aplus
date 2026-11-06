/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>
#include <aplus/module.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/vfs.h>


__noreturn
void kshutdown(int mode) {

#if DEBUG_LEVEL_INFO
    kprintf("core: shutting down with mode(%d)\n", mode);
#endif

    sched_dnit();
    smp_dnit();
    vfs_dnit();
    module_dnit();

#if DEBUG_LEVEL_INFO
    kprintf("core: bye! 👋\n");
#endif

    arch_reboot(mode);
}
