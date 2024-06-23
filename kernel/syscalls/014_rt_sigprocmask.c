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
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/vfs.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus/hal.h>



/***
 * Name:        rt_sigprocmask
 * Description: examine and change blocked signals
 * URL:         http://man7.org/linux/man-pages/man2/rt_sigprocmask.2.html
 *
 * Input Parameters:
 *  0: 0x0e
 *  1: int how
 *  2: sigset_t  * set
 *  3: sigset_t  * oset
 *  4: size_t sigsetsize
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    14, rt_sigprocmask, long sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset, size_t sigsetsize) {
        if (unlikely(set && !uio_check(set, R_OK)))
            return -EFAULT;

        if (unlikely(oset && !uio_check(oset, R_OK | W_OK)))
            return -EFAULT;



        DEBUG_ASSERT(current_task);

        shared_ptr_access(current_task->sighand, sighand, {
            if (oset) {
                uio_memcpy_s2u(oset, &sighand->sigmask, sigsetsize);
            }

            if (set) {

                sigset_t __safe_set;
                uio_memcpy_u2s(&__safe_set, set, sizeof(sigset_t));

                switch (how) {

                    case SIG_BLOCK:

                        for (size_t i = 0; i < sigsetsize; i += sizeof(unsigned long))
                            sighand->sigmask.__bits[i] |= __safe_set.__bits[i];

                        break;

                    case SIG_UNBLOCK:

                        for (size_t i = 0; i < sigsetsize; i += sizeof(unsigned long))
                            sighand->sigmask.__bits[i] &= ~__safe_set.__bits[i];

                        break;

                    case SIG_SETMASK:

                        for (size_t i = 0; i < sigsetsize; i += sizeof(unsigned long))
                            sighand->sigmask.__bits[i] = __safe_set.__bits[i];

                        break;

                    default:
                        return -EINVAL;
                }
            }


            for (size_t i = current_task->sigpending.size; i > 0; i--) {

                siginfo_t *info;
                if ((info = queue_pop(&current_task->sigpending))) {

                    if (unlikely(sighand->sigmask.__bits[info->si_signo / (sizeof(long) << 3)] & (1 << (info->si_signo % (sizeof(long) << 3)))))
                        queue_enqueue(&current_task->sigpending, info, 0);
                    else
                        queue_enqueue(&current_task->sigqueue, info, 0);
                }
            }
        });


        return 0;
    });
