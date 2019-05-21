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


#ifndef _APLUS_SYSCALL_H
#define _APLUS_SYSCALL_H

#include <aplus.h>

#define SYSCALL(x, y, z)                        \
    z                                           \
    __attribute__((section(".syscalls")))       \
    struct {                                    \
        int a;                                  \
        void* b;                                \
        char* name;                             \
    } __sc_##y = {                              \
        (int) x,                                \
        (void*) sys_##y,                        \
        (char*) #y                              \
    }; EXPORT(sys_##y) 


void syscall_init(void);
long syscall_invoke(long, long, long, long, long, long, long);

#endif