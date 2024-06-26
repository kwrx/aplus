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

#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>



/***
 * Name:        tgkill
 * Description: send a signal to a thread
 * URL:         http://man7.org/linux/man-pages/man2/tgkill.2.html
 *
 * Input Parameters:
 *  0: 0xea
 *  1: pid_t tgid
 *  2: pid_t pid
 *  3: int sig
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    234, tgkill, long sys_tgkill(pid_t tgid, pid_t tid, int sig) {
        siginfo_t siginfo;
        siginfo.si_signo = sig;
        siginfo.si_code  = SI_TKILL;
        siginfo.si_errno = 0;

        return sys_rt_tgsigqueueinfo(tgid, tid, sig, &siginfo);
    });
