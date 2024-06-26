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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>


extern long sys_clock_nanosleep(clockid_t, int, struct timespec*, struct timespec*);


/***
 * Name:        nanosleep
 * Description: high-resolution sleep
 * URL:         http://man7.org/linux/man-pages/man2/nanosleep.2.html
 *
 * Input Parameters:
 *  0: 0x23
 *  1: struct timespec  * rqtp
 *  2: struct timespec  * rmtp
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(35, nanosleep, long sys_nanosleep(struct timespec* rqtp, struct timespec* rmtp) { return sys_clock_nanosleep(CLOCK_MONOTONIC, 0, rqtp, rmtp); });
