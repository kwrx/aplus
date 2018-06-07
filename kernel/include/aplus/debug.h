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


#ifndef _DEBUG_H
#define _DEBUG_H

#include <aplus.h>
#include <aplus/task.h>
#include <libc.h>



#define LOG                     "[ LOG   ] "
#define WARN                    "[ WARN  ] "
#define INFO                    "[ INFO  ] "
#define ERROR                   "[ ERROR ] "
#define USER                    "[ USER  ] "


#ifndef __ASSEMBLY__
void debug_send(char value);
void debug_dump(void* context, char* errmsg, uintptr_t dump, uintptr_t errcode);
void debug_stacktrace(int);


#if !DEBUG
#define debug_send(a)               (void) 0
#define debug_dump(x, y, z, w)      (void) 0
#define debug_stacktrace(x)         (void) 0
#define KASSERT(x)
#define KASSERTF(x, y...)
#define kprintf(a, b...)
#else

int kprintf(const char* fmt, ...);

#define KASSERT(x)                                                                                                                                          \
    if(unlikely(!(x)))                                                                                                                                      \
        {                                                                                                                                                   \
            kprintf(ERROR "[#%d %s] %s(): Assertion \"%s\" failed in %s:%d\n", current_task->pid, current_task->name, __func__, #x, __FILE__, __LINE__);                                                    \
            debug_stacktrace(5);                                                                                                                            \
            for(;;);                                                                                                                                        \
        }

#define KASSERTF(x, y...)                                                                                                                                   \
    if(unlikely(!(x)))                                                                                                                                      \
        {                                                                                                                                                   \
            kprintf(ERROR "[#%d %s] %s(): Assertion \"%s\" failed in %s:%d\nDetails: ", current_task->pid, current_task->name, __func__, #x, __FILE__, __LINE__);                                           \
            kprintf(y);                                                                                                                                     \
            kprintf("\n");                                                                                                                                  \
            debug_stacktrace(5);                                                                                                                            \
            for(;;);                                                                                                                                        \
        }


#endif
#endif

#endif
