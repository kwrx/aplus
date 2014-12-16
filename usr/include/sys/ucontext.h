#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H

#include <sys/cdefs.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

struct _libc_fpreg {
	uint16_t significand[4];
	uint16_t exponent;
};

struct _libc_fpstate {
	uint32_t cw;
	uint32_t sw;
	uint32_t tag;
	uint32_t ipoff;
	uint32_t cssel;
	uint32_t dataoff;
	uint32_t datasel;
	struct _libc_fpreg _st[8];
	uint32_t status;
};


typedef uint32_t greg_t;
typedef struct {
	uint32_t gregs[19];
	struct _libc_fpstate* fpregs;
	uint32_t oldmask;
	uint32_t cr2;
} mcontext_t;

enum {
	REG_GS = 0,
	REG_FS,
	REG_ES,
	REG_DS,
	REG_EDI,
	REG_ESI,
	REG_EBP,
	REG_ESP,
	REG_EBX,
	REG_EDX,
	REG_ECX,
	REG_EAX,
	REG_TRAPNO,
	REG_ERR,
	REG_EIP,
	REG_CS,
	REG_EFL,
	REG_UESP,
	REG_SS,
};

typedef struct ucontext {
	uint32_t uc_flags;
	struct ucontext* uc_link;
	uint32_t uc_stack;
	mcontext_t uc_mcontext;
} ucontext_t;


#ifdef __cplusplus
}
#endif

#endif
