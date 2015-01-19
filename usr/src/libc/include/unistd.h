#ifndef _UNISTD_H
#define _UNISTD_H

#define STDIN_FILENO				0
#define STDOUT_FILENO				1
#define STDERR_FILENO				2

#define _XOPEN_VERSION				600
#define _XOPEN_XCU_VERSION			4

#define	_POSIX_ADVISORY_INFO		(-1)		/* [ADV] */
#define	_POSIX_ASYNCHRONOUS_IO		(-1)		/* [AIO] */
#define	_POSIX_BARRIERS			(-1)		/* [BAR] */
#define	_POSIX_CHOWN_RESTRICTED		200112L
#define	_POSIX_CLOCK_SELECTION		(-1)		/* [CS] */
#define	_POSIX_CPUTIME			(-1)		/* [CPT] */
#define	_POSIX_FSYNC			200112L		/* [FSC] */
#define	_POSIX_IPV6			200112L
#define	_POSIX_JOB_CONTROL		200112L
#define	_POSIX_MAPPED_FILES		200112L		/* [MF] */
#define	_POSIX_MEMLOCK			(-1)		/* [ML] */
#define	_POSIX_MEMLOCK_RANGE		(-1)		/* [MR] */
#define	_POSIX_MEMORY_PROTECTION	200112L		/* [MPR] */
#define	_POSIX_MESSAGE_PASSING		(-1)		/* [MSG] */
#define	_POSIX_MONOTONIC_CLOCK		(-1)		/* [MON] */
#define	_POSIX_NO_TRUNC			200112L
#define	_POSIX_PRIORITIZED_IO		(-1)		/* [PIO] */
#define	_POSIX_PRIORITY_SCHEDULING	(-1)		/* [PS] */
#define	_POSIX_RAW_SOCKETS		(-1)		/* [RS] */
#define	_POSIX_READER_WRITER_LOCKS	200112L		/* [THR] */
#define	_POSIX_REALTIME_SIGNALS		(-1)		/* [RTS] */
#define	_POSIX_REGEXP			200112L
#define	_POSIX_SAVED_IDS		200112L		/* XXX required */
#define	_POSIX_SEMAPHORES		(-1)		/* [SEM] */
#define	_POSIX_SHARED_MEMORY_OBJECTS	(-1)		/* [SHM] */
#define	_POSIX_SHELL			200112L
#define	_POSIX_SPAWN			(-1)		/* [SPN] */
#define	_POSIX_SPIN_LOCKS		(-1)		/* [SPI] */
#define	_POSIX_SPORADIC_SERVER		(-1)		/* [SS] */
#define	_POSIX_SYNCHRONIZED_IO		(-1)		/* [SIO] */
#define	_POSIX_THREAD_ATTR_STACKADDR	200112L		/* [TSA] */
#define	_POSIX_THREAD_ATTR_STACKSIZE	200112L		/* [TSS] */
#define	_POSIX_THREAD_CPUTIME		(-1)		/* [TCT] */
#define	_POSIX_THREAD_PRIO_INHERIT	(-1)		/* [TPI] */
#define	_POSIX_THREAD_PRIO_PROTECT	(-1)		/* [TPP] */
#define	_POSIX_THREAD_PRIORITY_SCHEDULING	(-1)	/* [TPS] */
#define	_POSIX_THREAD_PROCESS_SHARED	200112L		/* [TSH] */
#define	_POSIX_THREAD_SAFE_FUNCTIONS	200112L		/* [TSF] */
#define	_POSIX_THREAD_SPORADIC_SERVER	(-1)		/* [TSP] */
#define	_POSIX_THREADS			200112L		/* [THR] */
#define	_POSIX_TIMEOUTS			(-1)		/* [TMO] */
#define	_POSIX_TIMERS			(-1)		/* [TMR] */
#define	_POSIX_TRACE			(-1)		/* [TRC] */
#define	_POSIX_TRACE_EVENT_FILTER	(-1)		/* [TEF] */
#define	_POSIX_TRACE_INHERIT		(-1)		/* [TRI] */
#define	_POSIX_TRACE_LOG		(-1)		/* [TRL] */
#define	_POSIX_TYPED_MEMORY_OBJECTS	(-1)		/* [TYM] */
#ifndef _POSIX_VDISABLE
#define	_POSIX_VDISABLE			0xff		/* same as sys/termios.h */
#endif /* _POSIX_VDISABLE */

#if __DARWIN_C_LEVEL >= 199209L
#define	_POSIX2_C_BIND			200112L
#define	_POSIX2_C_DEV			200112L		/* c99 command */
#define	_POSIX2_CHAR_TERM		200112L
#define	_POSIX2_FORT_DEV		(-1)		/* fort77 command */
#define	_POSIX2_FORT_RUN		200112L
#define	_POSIX2_LOCALEDEF		200112L		/* localedef command */
#define	_POSIX2_PBS			(-1)
#define	_POSIX2_PBS_ACCOUNTING		(-1)
#define	_POSIX2_PBS_CHECKPOINT		(-1)
#define	_POSIX2_PBS_LOCATE		(-1)
#define	_POSIX2_PBS_MESSAGE		(-1)
#define	_POSIX2_PBS_TRACK		(-1)
#define	_POSIX2_SW_DEV			200112L
#define	_POSIX2_UPE			200112L	/* XXXX no fc, newgrp, tabs */
#endif /* __DARWIN_C_LEVEL */

#define	__ILP32_OFF32          (-1)
#define	__ILP32_OFFBIG         (1)
#define	__LP64_OFF64           (1)
#define	__LPBIG_OFFBIG         (1)

#if __DARWIN_C_LEVEL >= 200112L
#define	_POSIX_V6_ILP32_OFF32		__ILP32_OFF32
#define	_POSIX_V6_ILP32_OFFBIG		__ILP32_OFFBIG
#define	_POSIX_V6_LP64_OFF64		__LP64_OFF64
#define	_POSIX_V6_LPBIG_OFFBIG		__LPBIG_OFFBIG
#endif /* __DARWIN_C_LEVEL >= 200112L */

#if __DARWIN_C_LEVEL >= 200809L
#define	_POSIX_V7_ILP32_OFF32		__ILP32_OFF32
#define	_POSIX_V7_ILP32_OFFBIG		__ILP32_OFFBIG
#define	_POSIX_V7_LP64_OFF64		__LP64_OFF64
#define	_POSIX_V7_LPBIG_OFFBIG		__LPBIG_OFFBIG
#endif /* __DARWIN_C_LEVEL >= 200809L */

#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
#define	_V6_ILP32_OFF32             __ILP32_OFF32
#define	_V6_ILP32_OFFBIG            __ILP32_OFFBIG
#define	_V6_LP64_OFF64              __LP64_OFF64
#define	_V6_LPBIG_OFFBIG            __LPBIG_OFFBIG
#endif /* __DARWIN_C_LEVEL >= __DARWIN_C_FULL */

#if (__DARWIN_C_LEVEL >= 199506L && __DARWIN_C_LEVEL < 200809L) || __DARWIN_C_LEVEL >= __DARWIN_C_FULL
/* Removed in Issue 7 */
#define	_XBS5_ILP32_OFF32		    __ILP32_OFF32
#define	_XBS5_ILP32_OFFBIG		    __ILP32_OFFBIG
#define	_XBS5_LP64_OFF64		    __LP64_OFF64
#define	_XBS5_LPBIG_OFFBIG		    __LPBIG_OFFBIG
#endif /* __DARWIN_C_LEVEL < 200809L */

#if __DARWIN_C_LEVEL >= 199506L /* This really should be XSI */ 
#define	_XOPEN_CRYPT			(1)
#define	_XOPEN_ENH_I18N			(1)		/* XXX required */
#define	_XOPEN_LEGACY			(-1)	/* no ftime gcvt, wcswcs */
#define	_XOPEN_REALTIME			(-1)	/* no q'ed signals, mq_* */
#define	_XOPEN_REALTIME_THREADS		(-1)	/* no posix_spawn, et. al. */
#define	_XOPEN_SHM			(1)
#define	_XOPEN_STREAMS			(-1)   /* Issue 6 */
#define	_XOPEN_UNIX			(1)
#endif /* XSI */

/* configurable system variables */
#define	_SC_ARG_MAX			 1
#define	_SC_CHILD_MAX			 2
#define	_SC_CLK_TCK			 3
#define	_SC_NGROUPS_MAX			 4
#define	_SC_OPEN_MAX			 5
#define	_SC_JOB_CONTROL			 6
#define	_SC_SAVED_IDS			 7
#define	_SC_VERSION			 8
#define	_SC_BC_BASE_MAX			 9
#define	_SC_BC_DIM_MAX			10
#define	_SC_BC_SCALE_MAX		11
#define	_SC_BC_STRING_MAX		12
#define	_SC_COLL_WEIGHTS_MAX		13
#define	_SC_EXPR_NEST_MAX		14
#define	_SC_LINE_MAX			15
#define	_SC_RE_DUP_MAX			16
#define	_SC_2_VERSION			17
#define	_SC_2_C_BIND			18
#define	_SC_2_C_DEV			19
#define	_SC_2_CHAR_TERM			20
#define	_SC_2_FORT_DEV			21
#define	_SC_2_FORT_RUN			22
#define	_SC_2_LOCALEDEF			23
#define	_SC_2_SW_DEV			24
#define	_SC_2_UPE			25
#define	_SC_STREAM_MAX			26
#define	_SC_TZNAME_MAX			27

#if __DARWIN_C_LEVEL >= 199309L
#define	_SC_ASYNCHRONOUS_IO		28
#define	_SC_PAGESIZE			29
#define	_SC_MEMLOCK			30
#define	_SC_MEMLOCK_RANGE		31
#define	_SC_MEMORY_PROTECTION		32
#define	_SC_MESSAGE_PASSING		33
#define	_SC_PRIORITIZED_IO		34
#define	_SC_PRIORITY_SCHEDULING		35
#define	_SC_REALTIME_SIGNALS		36
#define	_SC_SEMAPHORES			37
#define	_SC_FSYNC			38
#define	_SC_SHARED_MEMORY_OBJECTS 	39
#define	_SC_SYNCHRONIZED_IO		40
#define	_SC_TIMERS			41
#define	_SC_AIO_LISTIO_MAX		42
#define	_SC_AIO_MAX			43
#define	_SC_AIO_PRIO_DELTA_MAX		44
#define	_SC_DELAYTIMER_MAX		45
#define	_SC_MQ_OPEN_MAX			46
#define	_SC_MAPPED_FILES		47	/* swap _SC_PAGESIZE vs. BSD */
#define	_SC_RTSIG_MAX			48
#define	_SC_SEM_NSEMS_MAX		49
#define	_SC_SEM_VALUE_MAX		50
#define	_SC_SIGQUEUE_MAX		51
#define	_SC_TIMER_MAX			52
#endif /* __DARWIN_C_LEVEL >= 199309L */

#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
#define	_SC_NPROCESSORS_CONF		57
#define	_SC_NPROCESSORS_ONLN		58
#endif /* __DARWIN_C_LEVEL >= __DARWIN_C_FULL */

#if __DARWIN_C_LEVEL >= 200112L
#define	_SC_2_PBS			59
#define	_SC_2_PBS_ACCOUNTING		60
#define	_SC_2_PBS_CHECKPOINT		61
#define	_SC_2_PBS_LOCATE		62
#define	_SC_2_PBS_MESSAGE		63
#define	_SC_2_PBS_TRACK			64
#define	_SC_ADVISORY_INFO		65
#define	_SC_BARRIERS			66
#define	_SC_CLOCK_SELECTION		67
#define	_SC_CPUTIME			68
#define	_SC_FILE_LOCKING		69
#define	_SC_GETGR_R_SIZE_MAX		70
#define	_SC_GETPW_R_SIZE_MAX		71
#define	_SC_HOST_NAME_MAX		72
#define	_SC_LOGIN_NAME_MAX		73
#define	_SC_MONOTONIC_CLOCK		74
#define	_SC_MQ_PRIO_MAX			75
#define	_SC_READER_WRITER_LOCKS		76
#define	_SC_REGEXP			77
#define	_SC_SHELL			78
#define	_SC_SPAWN			79
#define	_SC_SPIN_LOCKS			80
#define	_SC_SPORADIC_SERVER		81
#define	_SC_THREAD_ATTR_STACKADDR	82
#define	_SC_THREAD_ATTR_STACKSIZE	83
#define	_SC_THREAD_CPUTIME		84
#define	_SC_THREAD_DESTRUCTOR_ITERATIONS 85
#define	_SC_THREAD_KEYS_MAX		86
#define	_SC_THREAD_PRIO_INHERIT		87
#define	_SC_THREAD_PRIO_PROTECT		88
#define	_SC_THREAD_PRIORITY_SCHEDULING	89
#define	_SC_THREAD_PROCESS_SHARED	90
#define	_SC_THREAD_SAFE_FUNCTIONS	91
#define	_SC_THREAD_SPORADIC_SERVER	92
#define	_SC_THREAD_STACK_MIN		93
#define	_SC_THREAD_THREADS_MAX		94
#define	_SC_TIMEOUTS			95
#define	_SC_THREADS			96
#define	_SC_TRACE			97
#define	_SC_TRACE_EVENT_FILTER		98
#define	_SC_TRACE_INHERIT		99
#define	_SC_TRACE_LOG			100
#define	_SC_TTY_NAME_MAX		101
#define	_SC_TYPED_MEMORY_OBJECTS	102
#define	_SC_V6_ILP32_OFF32		103
#define	_SC_V6_ILP32_OFFBIG		104
#define	_SC_V6_LP64_OFF64		105
#define	_SC_V6_LPBIG_OFFBIG		106
#define	_SC_IPV6			118
#define	_SC_RAW_SOCKETS			119
#define	_SC_SYMLOOP_MAX			120
#endif /* __DARWIN_C_LEVEL >= 200112L */

#if __DARWIN_C_LEVEL >= 199506L /* Really XSI */
#define	_SC_ATEXIT_MAX			107
#define	_SC_IOV_MAX			56
#define	_SC_PAGE_SIZE			_SC_PAGESIZE
#define	_SC_XOPEN_CRYPT			108
#define	_SC_XOPEN_ENH_I18N		109
#define	_SC_XOPEN_LEGACY		110      /* Issue 6 */
#define	_SC_XOPEN_REALTIME		111      /* Issue 6 */
#define	_SC_XOPEN_REALTIME_THREADS	112  /* Issue 6 */
#define	_SC_XOPEN_SHM			113
#define	_SC_XOPEN_STREAMS		114      /* Issue 6 */
#define	_SC_XOPEN_UNIX			115
#define	_SC_XOPEN_VERSION		116
#define	_SC_XOPEN_XCU_VERSION		121
#endif /* XSI */

#if (__DARWIN_C_LEVEL >= 199506L && __DARWIN_C_LEVEL < 200809L) || __DARWIN_C_LEVEL >= __DARWIN_C_FULL
/* Removed in Issue 7 */
#define	_SC_XBS5_ILP32_OFF32		122
#define	_SC_XBS5_ILP32_OFFBIG		123
#define	_SC_XBS5_LP64_OFF64		124
#define	_SC_XBS5_LPBIG_OFFBIG		125
#endif /* __DARWIN_C_LEVEL <= 200809L */

#if __DARWIN_C_LEVEL >= 200112L
#define	_SC_SS_REPL_MAX			126
#define	_SC_TRACE_EVENT_NAME_MAX	127
#define	_SC_TRACE_NAME_MAX		128
#define	_SC_TRACE_SYS_MAX		129
#define	_SC_TRACE_USER_EVENT_MAX	130
#endif

#if __DARWIN_C_LEVEL < 200112L || __DARWIN_C_LEVEL >= __DARWIN_C_FULL
/* Removed in Issue 6 */
#define	_SC_PASS_MAX			131
#endif

#if __DARWIN_C_LEVEL >= 199209L
#ifndef _CS_PATH /* Defined in <sys/unistd.h> */
#define	_CS_PATH				1
#endif
#endif

#if __DARWIN_C_LEVEL >= 200112
#define	_CS_POSIX_V6_ILP32_OFF32_CFLAGS		2
#define	_CS_POSIX_V6_ILP32_OFF32_LDFLAGS	3
#define	_CS_POSIX_V6_ILP32_OFF32_LIBS		4
#define	_CS_POSIX_V6_ILP32_OFFBIG_CFLAGS	5
#define	_CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS	6
#define	_CS_POSIX_V6_ILP32_OFFBIG_LIBS		7
#define	_CS_POSIX_V6_LP64_OFF64_CFLAGS		8
#define	_CS_POSIX_V6_LP64_OFF64_LDFLAGS		9
#define	_CS_POSIX_V6_LP64_OFF64_LIBS		10
#define	_CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS	11
#define	_CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS	12
#define	_CS_POSIX_V6_LPBIG_OFFBIG_LIBS		13
#define	_CS_POSIX_V6_WIDTH_RESTRICTED_ENVS	14
#endif

#if (__DARWIN_C_LEVEL >= 199506L && __DARWIN_C_LEVEL < 200809L) || __DARWIN_C_LEVEL >= __DARWIN_C_FULL
/* Removed in Issue 7 */
#define	_CS_XBS5_ILP32_OFF32_CFLAGS		20
#define	_CS_XBS5_ILP32_OFF32_LDFLAGS		21
#define	_CS_XBS5_ILP32_OFF32_LIBS		22
#define	_CS_XBS5_ILP32_OFF32_LINTFLAGS		23
#define	_CS_XBS5_ILP32_OFFBIG_CFLAGS		24
#define	_CS_XBS5_ILP32_OFFBIG_LDFLAGS		25
#define	_CS_XBS5_ILP32_OFFBIG_LIBS		26
#define	_CS_XBS5_ILP32_OFFBIG_LINTFLAGS		27
#define	_CS_XBS5_LP64_OFF64_CFLAGS		28
#define	_CS_XBS5_LP64_OFF64_LDFLAGS		29
#define	_CS_XBS5_LP64_OFF64_LIBS		30
#define	_CS_XBS5_LP64_OFF64_LINTFLAGS		31
#define	_CS_XBS5_LPBIG_OFFBIG_CFLAGS		32
#define	_CS_XBS5_LPBIG_OFFBIG_LDFLAGS		33
#define	_CS_XBS5_LPBIG_OFFBIG_LIBS		34
#define	_CS_XBS5_LPBIG_OFFBIG_LINTFLAGS		35
#endif

#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
#define	_CS_DARWIN_USER_DIR			65536
#define	_CS_DARWIN_USER_TEMP_DIR		65537
#define	_CS_DARWIN_USER_CACHE_DIR		65538
#endif /* __DARWIN_C_LEVEL >= __DARWIN_C_FULL */


#ifdef __cplusplus
extern "C" {
#endif

void _exit(int);
int access(const char*, int);
unsigned int alarm(unsigned int);
int chdir(const char*);
int chown(const char*, uid_t, gid_t);

int close(int);
int lseek(int, off_t, int);
ssize_t read(int, void*, size_t);
ssize_t write(int, void*, size_t);

int dup(int);
int dup2(int, int);

int execl(const char*, const char*, ...);
int execle(const char*, const char*, ...);
int execlp(const char*, const char*, ...);
int execv(const char*, const char**);
int execve(const char*, const char**, const char**);
int execvp(const char*, const char**);

pid_t fork(void);

long fpathconf(int, int);

char* getcwd(char*, size_t);

gid_t getegid(void);
uid_t geteuid(void);
git_t getgid(void);
uid_t getgroups(int, gid_t[]);

char* getlogin(void);

pid_t getpgrp(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);

int isatty(void);
int link(const char*, const char*);

long pathconf(const char*, int);

int pause(void);

int pipe(int [2]);
int rmdir(const char*);

int setgid(git_t);
int setpgid(pid_t, pid_t);
pid_t setsid(void);
int setuid(uid_t);


unsigned int sleep(unsigned int);

long sysconf(int);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
char* ttyname(int);

int unlink(const char*);

#ifdef __cplusplus
}
#endif

#endif
