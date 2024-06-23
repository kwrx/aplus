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
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


/***
 * Name:        msgrcv
 * Description: System V message queue operations
 * URL:         http://man7.org/linux/man-pages/man2/msgrcv.2.html
 *
 * Input Parameters:
 *  0: 0x46
 *  1: int msqid
 *  2: struct msgbuf  * msgp
 *  3: size_t msgsz
 *  4: long msgtyp
 *  5: int msgflg
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

struct msgbuf;

SYSCALL(70, msgrcv, long sys_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg) { return -ENOSYS; });
