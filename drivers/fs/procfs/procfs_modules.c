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
#include <aplus/module.h>
#include <libc.h>

#include "procfs.h"


int procfs_modules_init(procfs_entry_t* e) {
    return 0;
}


int procfs_modules_update(procfs_entry_t* e) {
    memset(e->data, 0, strlen(e->data));

    list_each(m_queue, tmp) {
        char buf[128];
        memset(buf, 0, sizeof(buf));

        sprintf(buf, 
            "%s %d %d ",
            tmp->name,
            tmp->size,
            tmp->loaded
        );
        strcat(e->data, (const char*) buf);

        if(list_length(tmp->deps) == 0)
            strcat(e->data, "-");
        else {
            list_each(tmp->deps, d) {
                strcat(e->data, d);
                strcat(e->data, ",");
            }
        }

        memset(buf, 0, sizeof(buf));
        sprintf(buf, " Live %p\n", tmp->loaded_address);
        strcat(e->data, (const char*) buf);
    }
    

    e->size = strlen(e->data);
    return 0;
}