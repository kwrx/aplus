#ifndef _UCONTEXT_H
#define _UCONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

typedef void* mcontext_t;
typedef struct ucontext ucontext_t;

struct ucontext {
	struct ucontext* uc_link;
	sigset_t uc_sigmask;
	stack_t uc_stack;
	mcontext_t uc_mcontext;
};


#ifdef __cplusplus
}
#endif
#endif
