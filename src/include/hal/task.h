#ifndef _APLUS_HAL_TASK_H
#define _APLUS_HAL_TASK_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>



__BEGIN_DECLS

void arch_task_switch(void*, task_t*, task_t*);
void arch_task_spawn_init(void);

__END_DECLS

#endif
#endif
