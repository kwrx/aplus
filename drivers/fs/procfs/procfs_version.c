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
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_version_init(procfs_entry_t* e) {
    sprintf(e->data,
        "%s version %s (%s-gcc %d.%d.%d) %s %s\n",
        KERNEL_NAME,
        KERNEL_VERSION,
        TARGET, __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
        KERNEL_DATE,
        KERNEL_TIME
    );

    e->size = strlen(e->data);
    return 0;
}


int procfs_version_update(procfs_entry_t* e) {
    return 0;
}