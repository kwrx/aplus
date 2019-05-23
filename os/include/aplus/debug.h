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


#ifndef _APLUS_DEBUG_H
#define _APLUS_DEBUG_H

#if defined(KERNEL)
#include <aplus.h>

void arch_debug_init(void);
void arch_debug_putc(int);
void arch_debug_stacktrace(uintptr_t*, size_t);



#define ERROR           "[ ERROR ] "
#define INFO            "[ INFO  ] "
#define LOG             "[ LOG   ] "
#define WARN            "[ WARN  ] "


#define DEBUG_ASSERT(x) {                                                                           \
    if(unlikely(!(x))) {                                                                            \
        kprintf(ERROR "Assert failed \'%s\' in %s:%d <%s>\n", #x, __FILE__, __LINE__, __func__);    \
        core_stacktrace();                                                                          \
        for(;;);                                                                                    \
    }                                                                                               \
}


#define DEBUG_BREAKPOINT() {                                                                        \
    kprintf(INFO "Breakpoint! in %s:%d <%s>\n", __FILE__, __LINE__, __func__);                      \
    for(;;);                                                                                        \
}

#define DEBUG_WARNING(x) {                                                                          \
    if(unlikely(!(x))) {                                                                            \
        kprintf(WARN "Assert warning \'%s\' in %s:%d <%s>\n", #x, __FILE__, __LINE__, __func__);    \
    }                                                                                               \
}


int kprintf(const char*, ...);
void kpanic(const char*, ...);

void core_stacktrace(void);
#endif

#endif