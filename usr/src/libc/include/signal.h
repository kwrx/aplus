#ifndef _SIGNAL_H
#define _SIGNAL_H

#define NSIG			32

#define SIGHUP			1
#define SIGINT			2
#define SIGQUIT			3
#define SIGILL			4

#ifndef _POSIX_SOURCE
#define SIGIOT			SIGABRT
#define SIGEMT			7
#endif

#define SIGFPE			8
#define SIGKILL			9

#ifndef _POSIX_SOURCE
#define SIGBUS			10
#endif

#define SIGSEGV			11

#ifndef _POSIX_SOURCE
#define SIGSYS			12
#endif

#define SIGPIPE			13
#define SIGALRM			14
#define SIGTERM			15

#ifndef _POSIX_SOURCE
#define SIGURG			16
#endif

#define SIGSTOP			17
#define SIGTSTP			18
#define SIGCONT			19
#define SIGCHLD			20
#define SIGTTIN			21
#define SIGTTOU			22

#ifndef _POSIX_SOURCE
#define SIGIO			23
#define SIGXCPU			24
#define SIGXFSZ			25
#define SIGVTALRM		26
#define SIGPROF			27
#define SIGWINCH		28
#define SIGINFO			29
#endif

#define SIGUSR1			30
#define SIGUSR2			31





#define SIG_DFL			((void (*) (int)) 0)
#define SIG_IGN			((void (*) (int)) 1)
#define SIG_ERR			((void (*) (int)) -1)


typedef unsigned int sigset_t;

struct sigaction {
	void (*sa_handler) ();
	sigset_t sa_mask;
	int sa_flags;
};

#ifndef _POSIX_SOURCE
#define SA_ONSTACK			0x0001
#define SA_RESTART			0x0002
#define SA_DISABLE			0x0004
#endif

#define SA_NOCLDSTOP		0x0008


#define SIG_BLOCK			1
#define SIG_UNBLOCK			2
#define SIG_SETMASK			3

typedef void (*sig_t) __P((int));

struct sigaltstack {
	char* ss_base;
	int ss_size;
	int ss_flags;
};


#define MINSIGSTKSZ			4096
#define SIGSTKSZ			4096

struct sigvec {
	void (*sv_handler) ();
	int sv_mask;
	int sv_flags;
};


#define SV_ONSTACK			SA_ONSTACK
#define SV_INTERRUPT		SA_RESTART


struct sigstack {
	char** ss_sp;
	int ss_onstack;
};


#define sigmask(m)			(1 << ((m) - 1))
#define BADSIG				SIG_ERR


#ifdef __cplusplus
extern "C" {
#endif

void (*signal __P((int, void(*) __P((int))))) __P((int));

#ifdef __cplusplus
}
#endif

#endif
