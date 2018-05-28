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


#ifndef _SYSCALL_H
#define _SYSCALL_H

int syscall_init(void);
int syscall_register(int, void*);
int syscall_unregister(int);
void syscall_ack(void);


long syscall_handler(long, long, long, long, long, long);



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


#endif
