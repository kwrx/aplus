#include <errno.h>
#undef errno
int errno;

#define SYSCODE			"int $0x80"

#define syscall0(n, h)					\
	h() {								\
		int r, e;							\
		__asm__ __volatile__ (			\
			SYSCODE						\
			: "=a"(r), "=b"(e)	\
			: "a"(n)					\
		);								\
		errno = e;						\
		return r;						\
	}

#define syscall1(n, h)					\
	h(p1) {								\
		int r, e;							\
		__asm__ __volatile__ (			\
			SYSCODE						\
			: "=a"(r), "=b"(e)	\
			: "a"(n), "b"(p1)			\
		);								\
		errno = e;						\
		return r;						\
	}

#define syscall2(n, h)					\
	h(p1, p2) {							\
		int r, e;							\
		__asm__ __volatile__ (			\
			SYSCODE						\
			: "=a"(r), "=b"(e)	\
			: "a"(n), "b"(p1), "c"(p2)	\
		);								\
		errno = e;						\
		return r;						\
	}

#define syscall3(n, h)					\
	h(p1, p2, p3) {						\
		int r, e;							\
		__asm__ __volatile__ (			\
			SYSCODE						\
			: "=a"(r), "=b"(e)	\
			: "a"(n), "b"(p1), "c"(p2),	\
			  "d"(p3)					\
		);								\
		errno = e;						\
		return r;						\
	}

#define syscall4(n, h)					\
	h(p1, p2, p3, p4) {					\
		int r, e;							\
		__asm__ __volatile__ (			\
			SYSCODE						\
			: "=a"(r), "=b"(e)	\
			: "a"(n), "b"(p1), "c"(p2),	\
			  "d"(p3), "S"(p4)			\
		);								\
		errno = e;						\
		return r;						\
	}

#define syscall5(n, h)					\
	h(p1, p2, p3, p4, p5) {				\
		int r, e;							\
		__asm__ __volatile__ (			\
			SYSCODE						\
			: "=a"(r), "=b"(e)	\
			: "a"(n), "b"(p1), "c"(p2),	\
			  "d"(p3), "S"(p4), "D"(p5)	\
		);								\
		errno = e;						\
		return r;						\
	}


syscall1(0, _exit)
syscall1(1, close)
syscall3(2, execve)
syscall0(3, fork)
syscall2(4, fstat)
syscall0(5, getpid)
syscall1(6, isatty)
syscall2(7, kill)
syscall2(8, link)
syscall3(9, lseek)
syscall3(10, open)
syscall3(11, read)
syscall1(12, sbrk)
syscall2(13, stat)
syscall1(14, times)
syscall1(15, unlink)
syscall1(16, wait)
syscall3(17, write)
syscall2(18, gettimeofday)
syscall3(19, fcntl)
syscall3(20, ioctl)
syscall3(21, chown)
syscall4(22, clone)
syscall0(23, sched_yield)
syscall5(24, mount)
syscall1(25, umount)
syscall2(26, umount2)
syscall5(27, openpty)
syscall2(28, symlink)
syscall1(29, fchdir)
syscall1(30, pipe)
syscall3(31, waitpid)
syscall0(32, getppid)

#ifdef __aplus__
syscall2(80, aplus_readdir)
syscall2(81, aplus_getgroups)
syscall0(100, __get_argv)
syscall0(101, __get_envp)
syscall1(102, __install_sighandler);
#endif


