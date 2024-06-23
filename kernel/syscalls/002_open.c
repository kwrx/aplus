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

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <aplus/hal.h>





/***
 * Name:        open
 * Description: open and possibly create a file
 * URL:         http://man7.org/linux/man-pages/man2/open.2.html
 *
 * Input Parameters:
 *  0: 0x02
 *  1: const char  * filename
 *  2: int flags
 *  3: umode_t mode
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(2, open,
long sys_open (const char  * filename, int flags, mode_t mode) {
    
    extern long sys_openat (int dfd, const char  * filename, int flags, mode_t mode);
    
    return sys_openat(AT_FDCWD, filename, flags, mode);
    
});
