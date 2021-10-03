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
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/task.h>
#include <aplus/errno.h>




/***
 * Name:        rt_sigaction
 * Description: examine and change a signal action
 * URL:         http://man7.org/linux/man-pages/man2/rt_sigaction.2.html
 *
 * Input Parameters:
 *  0: 0x0d
 *  1: int
 *  2: const struct sigaction __user *
 *  3: struct sigaction __user *
 *  4: size_t
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

struct sigaction;

SYSCALL(13, rt_sigaction,
long sys_rt_sigaction (int signo, const struct ksigaction __user * act, struct ksigaction __user * oldact, size_t size) {
    
    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(current_task->sighand);


    if(unlikely(signo < 0))
        return -EINVAL;

    if(unlikely(signo > _NSIG - 1))
        return -EINVAL;

    if(unlikely(signo == SIGKILL))
        return -EINVAL;

    if(unlikely(signo == SIGSTOP))
        return -EINVAL;

    if(unlikely(act && !uio_check(act, R_OK | W_OK)))
        return -EFAULT;

    if(unlikely(oldact && !uio_check(oldact, R_OK | W_OK)))
        return -EFAULT;


    if(oldact)
        memcpy(oldact, &current_task->sighand->action[signo], sizeof(struct ksigaction));

    if(act)
        memcpy(&current_task->sighand->action[signo], act, sizeof(struct sigaction));


    return 0;

});
