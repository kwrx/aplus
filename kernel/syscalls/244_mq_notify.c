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
 * Name:        mq_notify
 * Description: register for notification when a message is available
 * URL:         http://man7.org/linux/man-pages/man2/mq_notify.2.html
 *
 * Input Parameters:
 *  0: 0xf4
 *  1: mqd_t mqdes
 *  2: const struct sigevent  * notification
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

typedef long mqd_t;
struct sigevent;

SYSCALL(244, mq_notify,
long sys_mq_notify (mqd_t mqdes, const struct sigevent  * notification) {
    return -ENOSYS;
});
