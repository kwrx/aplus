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
 * Name:        add_key
 * Description: add a key to the kernel's key management facility
 * URL:         http://man7.org/linux/man-pages/man2/add_key.2.html
 *
 * Input Parameters:
 *  0: 0xf8
 *  1: const char __user * _type
 *  2: const char __user * _description
 *  3: const void __user * _payload
 *  4: size_t plen
 *  5: key_serial_t destringid
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

typedef long key_serial_t;

SYSCALL(248, add_key,
long sys_add_key (const char __user * _type, const char __user * _description, const void __user * _payload, size_t plen, key_serial_t destringid) {
    return -ENOSYS;
});
