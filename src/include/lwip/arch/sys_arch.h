#ifndef _SYS_ARCH_H
#define _SYS_ARCH_H

#include <stdint.h>
#include "arch/cc.h"

typedef uint32_t sys_sem_t;
typedef uint32_t sys_mutex_t;
typedef int sys_thread_t;
typedef int sys_prot_t;

struct sys_mbox {
	struct sys_mbox* next;
	uint16_t size;
	uint16_t count;
	void** msg;
};

typedef struct sys_mbox sys_mbox_t;

#define SYS_MBOX_NULL	((void*) 0)
#define SYS_SEM_NULL	((void*) 0)


#define sys_sem_valid(s)			(1)
#define sys_sem_set_invalid(s)
#define sys_mbox_valid(m)			(1)
#define sys_mbox_set_invalid(m)

#endif
