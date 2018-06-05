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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(181, pwrite64,
int sys_pwrite64(int fd, void* buf, size_t count, off_t off) {
    if(unlikely(fd < 0)) {
        errno = EBADF;
        return -1;
    }
    
    off_t old = sys_lseek(fd, 0, SEEK_CUR);
    if(old < 0)
        return -1;

    if(sys_lseek(fd, off, SEEK_SET) < 0)
        return -1;
        
    int r = sys_write(fd, buf, count);
    sys_lseek(fd, old, SEEK_SET);

    return r;
});