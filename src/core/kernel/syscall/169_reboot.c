/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
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
 * Name:        reboot
 * Description: reboot or enable/disable Ctrl-Alt-Del
 * URL:         http://man7.org/linux/man-pages/man2/reboot.2.html
 *
 * Input Parameters:
 *  0: 0xa9
 *  1: int magic1
 *  2: int magic2
 *  3: unsigned int cmd
 *  4: void __user * arg
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(169, reboot,
long sys_reboot (int magic1, int magic2, unsigned int cmd, void __user * arg) {
    return -ENOSYS;
});
