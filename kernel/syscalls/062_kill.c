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
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>



/***
 * Name:        kill
 * Description: send signal to a process
 * URL:         http://man7.org/linux/man-pages/man2/kill.2.html
 *
 * Input Parameters:
 *  0: 0x3e
 *  1: pid_t pid
 *  2: int sig
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(62, kill,
long sys_kill (pid_t pid, int sig) {

    siginfo_t siginfo;
    siginfo.si_signo = sig;
    siginfo.si_code  = SI_USER;
    siginfo.si_errno = 0;

    return sys_rt_tgsigqueueinfo(pid, -1, sig, &siginfo);
});
