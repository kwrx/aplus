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


#include <aplus.h>
#include <aplus/debug.h>


void unittest_init(void) {

    extern int unittest_start;
    extern int unittest_end;

    struct {
        const char* name;
        const char* desc;
        const char* file;
        int (*run) (void);
    } *e = (void*) &unittest_start;


    int i = 0;
    int t = ((uintptr_t) &unittest_end - (uintptr_t) &unittest_start) / sizeof(*e);

    for(; (uintptr_t) e < (uintptr_t) &unittest_end; e++) {
        DEBUG_ASSERT(e->name);
        DEBUG_ASSERT(e->desc);
        DEBUG_ASSERT(e->file);
        DEBUG_ASSERT(e->run);


        kprintf("[%d/%d] Running %s\n", ++i, t, e->name);

        int r;
        if((r = e->run()) != E_OK)
            kprintf("[!] FAIL: %s on line %d\n", e->file, r);

    }

}