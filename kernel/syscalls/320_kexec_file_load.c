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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


/***
 * Name:        kexec_file_load
 * Description: load a new kernel for later execution
 * URL:         http://man7.org/linux/man-pages/man2/kexec_file_load.2.html
 *
 * Input Parameters:
 *  0: 0x140
 *  1: int kernel_fd
 *  2: int initrd_fd
 *  3: unsigned long cmdline_len
 *  4: const char  * cmdline_ptr
 *  5: unsigned long flags
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(320, kexec_file_load,
long sys_kexec_file_load (int kernel_fd, int initrd_fd, unsigned long cmdline_len, const char  * cmdline_ptr, unsigned long flags) {
    return -ENOSYS;
});
