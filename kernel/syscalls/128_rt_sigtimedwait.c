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
 * Name:        rt_sigtimedwait
 * Description: synchronously wait for
       queued signals
 * URL:         http://man7.org/linux/man-pages/man2/rt_sigtimedwait.2.html
 *
 * Input Parameters:
 *  0: 0x80
 *  1: const sigset_t  * uthese
 *  2: siginfo_t  * uinfo
 *  3: const struct timespec  * uts
 *  4: size_t sigsetsize
 *
 * Auto-generated by scripts/gen-syscalls.js
 */


SYSCALL(128, rt_sigtimedwait,
long sys_rt_sigtimedwait (const sigset_t  * uthese, siginfo_t  * uinfo, const struct timespec  * uts, size_t sigsetsize) {
    return -ENOSYS;
});
