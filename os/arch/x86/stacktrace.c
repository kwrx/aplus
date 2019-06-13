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
#include <aplus/mm.h>

void arch_debug_stacktrace(uintptr_t* frames, size_t count) {
    struct stack {
        struct stack* bp;
        uintptr_t ip;
    } *frame;

#if defined(__x86_64__)
    __asm__ __volatile__ ("mov rax, rbp" : "=a"(frame));
#else
    __asm__ __volatile__ ("mov eax, ebp" : "=a"(frame));
#endif

    int i;
    for(i = 0; frame && i < count; i++) {
        frames[i] = 0;

#if defined(__x86_64__)
        return; /* TODO: Add support for stacktrace in x86-64 */
#endif
        
        if(unlikely(!ptr_check(frame, R_OK)))
            break;

        frames[i] = frame->ip;
        frame = frame->bp;
    }
}