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


#if defined(__x86_64__)
#include <stdint.h>
#include <errno.h>

#pragma GCC diagnostic ignored      "-Wimplicit-int"
#define SYSCALL                     "int $0xFE"

#define __SC_0(x, y)                    \
    __attribute__((weak))               \
    long y (void) {                     \
        long r;                         \
        __asm__ __volatile__ (          \
            SYSCALL                     \
            : "=a"(r)                   \
            : "a"(x)                    \
        );                              \
        if(r < 0)                       \
            return errno = -r, -1;      \
        return r;                       \
    }

#define __SC_1(x, y)                    \
    __attribute__((weak))               \
    long y (p0) {                       \
        long r;                         \
        __asm__ __volatile__ (          \
            SYSCALL                     \
            : "=a"(r)                   \
            : "a"(x), "b"(p0)           \
        );                              \
        if(r < 0)                       \
            return errno = -r, -1;      \
        return r;                       \
    }

#define __SC_2(x, y)                    \
    __attribute__((weak))               \
    long y (p0, p1) {                   \
        long r;                         \
        __asm__ __volatile__ (          \
            SYSCALL                     \
            : "=a"(r)                   \
            : "a"(x), "b"(p0),          \
              "c"(p1)                   \
        );                              \
        if(r < 0)                       \
            return errno = -r, -1;      \
        return r;                       \
    }

#define __SC_3(x, y)                    \
    __attribute__((weak))               \
    long y (p0, p1, p2) {               \
        long r;                         \
        __asm__ __volatile__ (          \
            SYSCALL                     \
            : "=a"(r)                   \
            : "a"(x), "b"(p0),          \
              "c"(p1), "d"(p2)          \
        );                              \
        if(r < 0)                       \
            return errno = -r, -1;      \
        return r;                       \
    }

#define __SC_4(x, y)                    \
    __attribute__((weak))               \
    long y (p0, p1, p2, p3) {           \
        long r;                         \
        __asm__ __volatile__ (          \
            SYSCALL                     \
            : "=a"(r)                   \
            : "a"(x), "b"(p0),          \
              "c"(p1), "d"(p2),         \
              "S"(p3)                   \
        );                              \
        if(r < 0)                       \
            return errno = -r, -1;      \
        return r;                       \
    }

#define __SC_5(x, y)                    \
    __attribute__((weak))               \
    long y (p0, p1, p2, p3, p4) {       \
        long r;                         \
        __asm__ __volatile__ (          \
            SYSCALL                     \
            : "=a"(r)                   \
            : "a"(x), "b"(p0),          \
              "c"(p1), "d"(p2),         \
              "S"(p3), "D"(p4)          \
        );                              \
        if(r < 0)                       \
            return errno = -r, -1;      \
        return r;                       \
    }
        
#define __SC_6(x, y)                    \
    __attribute__((weak))               \
    long y (p0, p1, p2, p3, p4, p5) {   \
        int r, e;                       \
        __asm__ __volatile__ (          \
            "mov %0, %%r8"              \
            :: "r"(p5)                  \
        );                              \
                                        \
        __asm__ __volatile__ (          \
            SYSCALL                     \
            : "=a"(r)                   \
            : "a"(x), "b"(p0),          \
              "c"(p1), "d"(p2),         \
              "S"(p3), "D"(p4),         \
        );                              \
        if(r < 0)                       \
            return errno = -r, -1;      \
        return r;                       \
    }


#define SC(x, y, z)                     \
    __SC_##x (y, z)



SC(2, 4, stat)
SC(2, 5, fstat)
SC(2, 6, lstat)
SC(1, 63, uname)

/* Extension */
SC(1, 500, __exit)
SC(1, 501, sbrk)
SC(0, 503, fork)
SC(3, 504, _execve)
SC(2, 505, statvfs)
SC(1, 506, nice)

#include "../syscall.tbl.h"
#endif