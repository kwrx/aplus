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
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <libc.h>

#if DEBUG

int
kprintf(const char *fmt, ...) {
    char buf[BUFSIZ] = {0};
    char bfmt[BUFSIZ] = {0};
    
    sprintf(bfmt, "[%8f]%s", (double) timer_getus() / 1000000, fmt);

    va_list args;
    va_start(args, fmt);
    int out = vsprintf(buf, bfmt, args);
    
    
    int i;
    for(i = 0; i < out; i++)
        debug_send(buf[i]);


    va_end(args);
    return out;
}


EXPORT(kprintf);

#endif
