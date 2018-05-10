#ifndef _SYS_WAIT_H
# define _SYS_WAIT_H

#include <sys/types.h>



#define _WSTATUS(x)                 (x & 0177)
#define _WSTOPPED                   0177


#define WIFSTOPPED(x)               (_WSTATUS(x) == _WSTOPPED)
#define WSTOPSIG(x)                 (x >> 8)
#define WIFSIGNALED(x)              (_WSTATUS(x) != _WSTOPPED && _WSTATUS(x) != 0)
#define WTERMSIG(x)                 (_WSTATUS(x))
#define WIFEXITED(x)                (_WSTATUS(x) == 0)
#define WEXITSTATUS(x)              (x >> 8)

#ifndef _POSIX_SOURCE
#define WCOREFLAG                   0200
#define WCOREDUMP(x)                (x & WCOREFLAG)
#define W_EXITCODE(ret, sig)        ((ret) << 8 | sig)
#define W_STOPCODE(sig)             ((sig) << 8 | _WSTOPPED)

#define WAIT_ANY                    (-1)
#define WAIT_MYPGRP                 (0)
#endif


#define WNOHANG                     1
#define WUNTRACED                   2



#ifdef __cplusplus
extern "C" {
#endif

pid_t wait(int* status);
pid_t waitpid(pid_t, int*, int);

#ifdef __cplusplus
}
#endif
#endif

