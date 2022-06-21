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

#define _GNU_SOURCE
#include <sched.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


/***
 * Name:        clone
 * Description: create a child process
 * URL:         http://man7.org/linux/man-pages/man2/clone.2.html
 *
 * Input Parameters:
 *  0: 0x38
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */
SYSCALL(56, clone,
#if defined(__x86_64__)
long sys_clone(unsigned long flags, void __user *stack,
           int __user *parent_tid, int __user *child_tid,
           unsigned long tls)

#else
long sys_clone(unsigned long flags, void *stack,
          int __user *parent_tid, unsigned long tls,
          int __user *child_tid)

#endif
{


    if(unlikely(parent_tid && !uio_check(parent_tid, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(child_tid && !uio_check(child_tid, R_OK | W_OK)))
        return -EFAULT;


    struct kclone_args args = {
        .flags          = (uint64_t) (flags & ~CSIGNAL),
        .pidfd          = (uint64_t) (parent_tid ? uio_get_ptr(parent_tid) : 0),
        .parent_tid     = (uint64_t) (parent_tid ? uio_get_ptr(parent_tid) : 0),
        .child_tid      = (uint64_t) (child_tid  ? uio_get_ptr(child_tid)  : 0),
        .exit_signal    = (uint64_t) (flags & CSIGNAL),
        .stack          = (uint64_t) stack,
        .tls            = (uint64_t) tls,
    };

    
    pid_t pid;

    if((pid = do_fork(&args, sizeof(args))) < 0)
        return -errno;

    return pid;
    
});