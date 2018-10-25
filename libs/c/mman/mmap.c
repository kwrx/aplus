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


#include <sys/mman.h>

extern void* __mmap(uintptr_t[6]);

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
    uintptr_t p[6];
    p[0] = (uintptr_t) addr;
    p[1] = len;
    p[2] = prot;
    p[3] = flags;
    p[4] = fd;
    p[5] = offset;
    return __mmap(p);
}