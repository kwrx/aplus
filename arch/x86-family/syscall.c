/*
 * Author:
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


#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>

#include <arch/x86/acpi.h>
#include <arch/x86/cpu.h>



static inline long __syscall(unsigned long no, long p0, long p1, long p2, long p3, long p4, long p5) {

    long r;

#if defined(__x86_64__)

    register long r10 __asm__("r10") = p3;
    register long r8 __asm__("r8")   = p4;
    register long r9 __asm__("r9")   = p5;

    __asm__ __volatile__("int $0xFE" : "=a"(r) : "a"(no), "D"(p0), "S"(p1), "d"(p2), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");

#else

    (void)p5;

    __asm__ __volatile__("int $0xFE" : "=a"(r) : "a"(no), "b"(p0), "c"(p1), "d"(p2), "S"(p3), "D"(p4) : "memory");

#endif

    return r;
}

long __arch_syscall0(unsigned long n) {
    return __syscall(n, 0, 0, 0, 0, 0, 0);
}

long __arch_syscall1(unsigned long n, long a) {
    return __syscall(n, a, 0, 0, 0, 0, 0);
}

long __arch_syscall2(unsigned long n, long a, long b) {
    return __syscall(n, a, b, 0, 0, 0, 0);
}

long __arch_syscall3(unsigned long n, long a, long b, long c) {
    return __syscall(n, a, b, c, 0, 0, 0);
}

long __arch_syscall4(unsigned long n, long a, long b, long c, long d) {
    return __syscall(n, a, b, c, d, 0, 0);
}

long __arch_syscall5(unsigned long n, long a, long b, long c, long d, long e) {
    return __syscall(n, a, b, c, d, e, 0);
}

long __arch_syscall6(unsigned long n, long a, long b, long c, long d, long e, long f) {
    return __syscall(n, a, b, c, d, e, f);
}


TEST(x86_syscall_test, {
    DEBUG_ASSERT(__arch_syscall6(404, 1, 2, 3, 4, 5, 6) == 0xDEADBEEF);
});
