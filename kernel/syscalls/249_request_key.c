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
 * Name:        request_key
 * Description: request a key from the kernel's key management facility
 * URL:         http://man7.org/linux/man-pages/man2/request_key.2.html
 *
 * Input Parameters:
 *  0: 0xf9
 *  1: const char  * _type
 *  2: const char  * _description
 *  3: const char  * _callout_info
 *  4: key_serial_t destringid
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

typedef long key_serial_t;

SYSCALL(249, request_key, long sys_request_key(const char* _type, const char* _description, const char* _callout_info, key_serial_t destringid) { return -ENOSYS; });
