/*
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _APLUS_LINKAGE_H
#define _APLUS_LINKAGE_H

#ifdef __cplusplus
    #define asmlinkage extern "C"
#else
    #define asmlinkage
#endif


#ifdef __ASSEMBLY__

    #define ENTRY(proc) \
        .globl proc;    \
        .align 16, 0x90 proc:

    #define END(proc) .size proc, .- proc

    #define ENDPROC(proc)      \
        .type proc, @function; \
        END(proc)


    #if defined(__x86_64__)
        #define STDCALL(func)  \
            pushq % rbp;       \
            movq % rsp, % rbp; \
            callq func;        \
            popq % rbp;
    #endif
#endif


#endif
