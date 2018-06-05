#ifndef _SYS_RESOURCE_H_
#define _SYS_RESOURCE_H_

#include <sys/time.h>

#define	RUSAGE_SELF	0		/* calling process */
#define	RUSAGE_CHILDREN	-1		/* terminated child processes */


typedef unsigned long rlim_t;

struct rusage {
  	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
};

struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

int	getrusage (int, struct rusage*);

#endif

