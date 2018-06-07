/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#include <stdint.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wimplicit-int"
#define SYSCALL		"int $0x80"

#define __SC_0(x, y)				\
	__attribute__((weak))			\
	long y (void) {					\
		int r, e;					\
		__asm__ __volatile__ (		\
			SYSCALL					\
			: "=a"(r), "=b"(e)		\
			: "a"(x)				\
		);							\
		errno = e;					\
		return r;					\
	}

#define __SC_1(x, y)				\
	__attribute__((weak))			\
	long y (p0) {					\
		int r, e;					\
		__asm__ __volatile__ (		\
			SYSCALL					\
			: "=a"(r), "=b"(e)		\
			: "a"(x), "b"(p0)		\
		);							\
		errno = e;					\
		return r;					\
	}

#define __SC_2(x, y)				\
	__attribute__((weak))			\
	long y (p0, p1) {				\
		int r, e;					\
		__asm__ __volatile__ (		\
			SYSCALL					\
			: "=a"(r), "=b"(e)		\
			: "a"(x), "b"(p0),		\
			  "c"(p1)				\
		);							\
		errno = e;					\
		return r;					\
	}

#define __SC_3(x, y)				\
	__attribute__((weak))			\
	long y (p0, p1, p2) {			\
		int r, e;					\
		__asm__ __volatile__ (		\
			SYSCALL					\
			: "=a"(r), "=b"(e)		\
			: "a"(x), "b"(p0),		\
			  "c"(p1), "d"(p2)		\
		);							\
		errno = e;					\
		return r;					\
	}

#define __SC_4(x, y)				\
	__attribute__((weak))			\
	long y (p0, p1, p2, p3) {		\
		int r, e;					\
		__asm__ __volatile__ (		\
			SYSCALL					\
			: "=a"(r), "=b"(e)		\
			: "a"(x), "b"(p0),		\
			  "c"(p1), "d"(p2),		\
			  "S"(p3)				\
		);							\
		errno = e;					\
		return r;					\
	}

#define __SC_5(x, y)				\
	__attribute__((weak))			\
	long y (p0, p1, p2, p3, p4) {	\
		int r, e;					\
		__asm__ __volatile__ (		\
			SYSCALL					\
			: "=a"(r), "=b"(e)		\
			: "a"(x), "b"(p0),		\
			  "c"(p1), "d"(p2),		\
			  "S"(p3), "D"(p4)		\
		);							\
		errno = e;					\
		return r;					\
	}
	
#define __SC_6(x, y)				\
	__attribute__((weak))			\
	long __##y (p0) {				\
		int r, e;					\
		__asm__ __volatile__ (		\
			SYSCALL					\
			: "=a"(r), "=b"(e)		\
			: "a"(x), "b"(p0)		\
		);							\
		errno = e;					\
		return r;					\
	}								\
	__attribute__((weak))			\
	long y (p0, p1, p2, p3, p4, p5) {	\
		uintptr_t p[] = { 			\
			p0, p1, p2, p3, p4, p5	\
		}; return __##y(&p);		\
	}

#define SC(x, y, z)					\
	__SC_##x (y, z)


/* Linux-like layer */
SC(0, 0, __restart_syscall)
SC(1, 1, __exit)
SC(0, 2, fork)
SC(3, 3, read)
SC(3, 4, write)
SC(3, 5, open)
SC(1, 6, close)
SC(3, 7, waitpid)
SC(2, 9, link)
SC(1, 10, unlink)
SC(3, 11, execve)
SC(3, 11, _execve)
SC(1, 12, chdir)
SC(1, 13, time)
SC(3, 14, mknod)
SC(2, 15, chmod)
SC(3, 19, lseek)
SC(0, 20, getpid)
SC(5, 21, mount)
SC(1, 22, umount)
SC(1, 27, alarm)
SC(0, 29, pause)
SC(1, 34, nice)
SC(0, 36, sync)
SC(2, 37, kill)
SC(2, 38, rename)
SC(1, 41, dup)
SC(1, 43, times)
SC(1, 45, brk)
SC(2, 52, umount2)
SC(3, 54, ioctl)
SC(3, 55, fcntl)
SC(2, 57, setpgid)
SC(1, 60, umask)
SC(1, 61, chroot)
SC(0, 64, getppid)
SC(0, 65, getpgrp)
SC(0, 66, setsid)
SC(1, 73, sigpending)
SC(2, 74, sethostname)
SC(2, 75, setrlimit)
SC(2, 77, getrusage)
SC(2, 78, gettimeofday)
SC(2, 79, settimeofday)
SC(2, 83, symlink)
SC(3, 85, readlink)
SC(2, 91, munmap)
SC(2, 92, truncate)
SC(2, 93, ftruncate)
SC(2, 94, fchmod)
SC(2, 96, getpriority)
SC(2, 97, setpriority)
SC(3, 104, setitimer)
SC(2, 105, getitimer)
SC(2, 106, stat)
SC(2, 107, lstat)
SC(2, 108, fstat)
SC(0, 111, vhangup)
SC(4, 114, wait4)
SC(1, 118, fsync)
SC(4, 120, clone)
SC(1, 122, uname)
SC(3, 125, mprotect)
SC(3, 126, sigprocmask)
SC(3, 128, init_module)
SC(2, 129, delete_module)
SC(1, 132, getpgid)
SC(1, 133, fchdir)
SC(5, 140, _llseek)
SC(3, 141, getdents)
SC(5, 142, select)
SC(2, 143, flock)
SC(3, 144, msync)
SC(3, 145, readv)
SC(3, 146, writev)
SC(1, 147, getsid)
SC(1, 148, fdatasync)
SC(2, 150, mlock)
SC(2, 151, munlock)
SC(1, 152, mlockall)
SC(0, 153, munlockall)
SC(2, 154, sched_setparam)
SC(2, 155, sched_getparam)
SC(3, 156, sched_setscheduler)
SC(1, 157, sched_getscheduler)
SC(0, 158, sched_yield)
SC(1, 159, sched_get_priority_max)
SC(1, 160, sched_get_priority_min)
SC(2, 161, sched_rr_get_interval)
SC(2, 162, nanosleep)
SC(3, 168, poll)
//SC(4, 177, rt_sigtimedwait)
//SC(3, 178, rt_sigqueueinfo)
SC(4, 187, sendfile)
SC(0, 190, vfork)
SC(2, 191, getrlimit)
SC(3, 198, lchown)
SC(0, 199, getuid)
SC(0, 200, getgid)
SC(0, 201, geteuid)
SC(0, 202, getegid)
SC(2, 203, setreuid)
SC(2, 204, setregid)
SC(2, 205, getgroups)
SC(2, 206, setgroups)
SC(3, 207, fchown)
SC(3, 208, setresuid)
SC(3, 209, getresuid)
SC(3, 210, setresgid)
SC(3, 211, getresgid)
SC(3, 212, chown)
SC(1, 213, setuid)
SC(1, 214, setgid)
SC(1, 215, setfsuid)
SC(1, 216, setfsgid)
SC(2, 265, clock_gettime)
SC(2, 266, clock_getres)
SC(2, 271, utimes)


/* Extension */
SC(1, 800, isatty)
SC(1, 801, sbrk)
SC(5, 802, openpty)
SC(2, 803, statvfs)
SC(2, 804, fstatvfs)
SC(1, 805, sysconf)
SC(2, 806, mkfifo)


/* Socket */
SC(3, 700, socket)
SC(3, 701, bind)
SC(2, 702, shutdown)
SC(3, 703, accept)
SC(3, 704, getpeername)
SC(3, 705, getsockname)
SC(5, 706, getsockopt)
SC(5, 707, setsockopt)
SC(3, 708, connect)
SC(2, 709, listen)
SC(4, 710, recv)
SC(4, 711, send)
SC(6, 712, recvfrom)
SC(6, 713, sendto)
SC(1, 714, gethostbyname)



SC(1, 900, __install_sighandler)

/* aPlus IPC */
SC(3, 950, ipc_msg_send)
SC(3, 951, ipc_msg_recv)


#define __bswap(a, x, y)                \
    y x (y n) {                         \
        return __builtin_bswap##a(n);   \
    }
    
__bswap(16, htons, unsigned short)
__bswap(16, ntohs, unsigned short)
__bswap(32, htonl, unsigned int)
__bswap(32, ntohl, unsigned int)




