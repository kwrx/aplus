#ifndef _COREUTILS_H
#define _COREUTILS_H

#include <sched.h>

#if 0
#define do_async(x, y)		\
	if(fork() == 0)			\
		{ x (y); }


#define do_sync(x, y)		\
	if(fork() == 0)			\
		{ x (y); }			\
	else
		wait(NULL)
#endif


#define do_async(x, y)							\
	clone(x, NULL, 	CLONE_FILES	|				\
					CLONE_FS	|				\
					CLONE_SIGHAND);

#define do_sync(x, y)							\
	{ clone(x, NULL, 	CLONE_FILES		|		\
						CLONE_FS		|		\
						CLONE_SIGHAND);			\
		wait(NULL);	}

#endif
