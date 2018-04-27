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


/* Standard */

 SC(1, 0, _exit)
SC(1, 1, close)
SC(3, 2, execve)
SC(3, 2, _execve)
SC(0, 3, fork)
SC(2, 4, fstat)
SC(0, 5, getpid)
SC(1, 6, isatty)
SC(2, 7, kill)
SC(2, 8, link)
SC(3, 9, lseek)
SC(3, 10, open)
SC(3, 11, read)
SC(1, 12, sbrk)
SC(2, 13, stat)
SC(1, 14, times)
SC(1, 15, unlink)
SC(1, 16, wait)
SC(3, 17, write)
SC(2, 18, clock_gettime)
SC(3, 19, fcntl)
SC(3, 20, ioctl)
SC(3, 21, chown)
SC(4, 22, clone)
SC(0, 23, sched_yield)
SC(5, 24, mount)
SC(1, 25, umount)
SC(2, 26, umount2)
SC(5, 27, openpty)
SC(2, 28, symlink)
SC(1, 29, fchdir)
SC(2, 30, mkfifo)
SC(3, 31, waitpid)
SC(0, 32, getppid)
SC(1, 33, uname)
SC(6, 34, mmap)
SC(2, 35, munmap)
SC(3, 36, readlink)
SC(1, 37, chdir)
SC(2, 38, lstat)
SC(1, 39, chroot)
SC(3, 40, getdents)
SC(1, 41, umask)
SC(2, 42, chmod)
SC(0, 43, getuid)
SC(0, 44, getgid)
SC(0, 45, getsid)
SC(1, 46, alarm)
SC(2, 47, setpgid)
SC(1, 48, getpgid)
SC(3, 49, sigprocmask)
SC(0, 50, setsid)
SC(1, 51, setuid)
SC(1, 52, setgid)
SC(2, 53, clock_getres)
SC(0, 54, sync)
SC(1, 55, fsync)
SC(2, 56, nanosleep)
SC(0, 57, pause)
SC(5, 58, select)
SC(1, 59, dup)
SC(1, 60, nice)

/* Socket */
SC(3, 70, socket)
SC(3, 71, bind)
SC(2, 72, shutdown)
SC(3, 73, accept)
SC(3, 74, getpeername)
SC(3, 75, getsockname)
SC(5, 76, getsockopt)
SC(5, 77, setsockopt)
SC(3, 78, connect)
SC(2, 79, listen)
SC(4, 80, recv)
SC(4, 81, send)
SC(6, 82, recvfrom)
SC(6, 83, sendto)
SC(1, 84, gethostbyname)
SC(3, 85, poll)
SC(3, 86, fchown)
SC(2, 87, fchmod)

/* System */
SC(1, 90, sysconf)

/* CRT0 */
SC(0, 100, __get_argv)				/* Deprecated */
SC(0, 101, __get_envp)				/* Deprecated */
SC(1, 102, __install_sighandler)

/* aPlus IPC */
SC(3, 105, msg_send)
SC(3, 106, msg_recv)


#define __bswap(a, x, y)                \
    y x (y n) {                         \
        return __builtin_bswap##a(n);   \
    }
    
__bswap(16, htons, unsigned short)
__bswap(16, ntohs, unsigned short)
__bswap(32, htonl, unsigned int)
__bswap(32, ntohl, unsigned int)




