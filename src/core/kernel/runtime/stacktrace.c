/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/elf.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>





void runtime_stacktrace(void) {

    uintptr_t frames[10] = { 0 };
    arch_debug_stacktrace((uintptr_t*) &frames, sizeof(frames) / sizeof(uintptr_t));


    int i;
    for(i = 1; i < sizeof(frames) / sizeof(uintptr_t); i++) {
        
        if(!frames[i])
            break;

        if(!ptr_check(frames[i], R_OK))
            break;

        const char* s;
        if((s = runtime_get_name(frames[i])))
            kprintf("[%d] %8p <%s>\n", i, frames[i], s);
        else
            kprintf("[%d] %8p <%s>\n", i, frames[i], "unknown");

    }

}
