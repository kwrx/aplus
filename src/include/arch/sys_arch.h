#ifndef _SYS_ARCH_H
#define _SYS_ARCH_H

#include <stdint.h>
#include "arch/cc.h"

typedef uint32_t sys_mutex_t;
typedef int sys_thread_t;
typedef int sys_prot_t;


struct sys_sem;
typedef struct sys_sem* sys_sem_t;

struct sys_mbox;
typedef struct sys_mbox* sys_mbox_t;


#define LWIP_COMPAT_MUTEX 1

#define SYS_MBOX_NULL	NULL
#define SYS_SEM_NULL	NULL


#define sys_sem_valid(s)			(((s) != NULL) && (*(s) != NULL))
#define sys_sem_set_invalid(s)		do { if(*(s) != NULL) { *(s) = NULL; } } while(0)

#define sys_mbox_valid(s)			(((s) != NULL) && (*(s) != NULL))
#define sys_mbox_set_invalid(s)		do { if(*(s) != NULL) { *(s) = NULL; } } while(0)

#endif
