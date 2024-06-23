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
 * Name:        mq_timedsend
 * Description: send a message to a message queue
 * URL:         http://man7.org/linux/man-pages/man2/mq_timedsend.2.html
 *
 * Input Parameters:
 *  0: 0xf2
 *  1: mqd_t mqdes
 *  2: const char  * msg_ptr
 *  3: size_t msg_len
 *  4: unsigned int msg_prio
 *  5: const struct timespec  * abs_timeout
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

typedef long mqd_t;

SYSCALL(242, mq_timedsend,
long sys_mq_timedsend (mqd_t mqdes, const char  * msg_ptr, size_t msg_len, unsigned int msg_prio, const struct timespec  * abs_timeout) {
    return -ENOSYS;
});
