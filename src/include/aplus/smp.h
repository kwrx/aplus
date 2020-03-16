#ifndef _APLUS_SMP_H
#define _APLUS_SMP_H

#ifndef __ASSEMBLY__
#include <time.h>
#include <sys/cdefs.h>
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/task.h>

#define SMP_CPU_MAX                     256
#define SMP_CPU_MAX_FEATURES            64
#define SMP_CPU_BOOTSTRAP_ID            0

#define SMP_CPU_FLAGS_AVAILABLE         (1 << 0)
#define SMP_CPU_FLAGS_ENABLED           (1 << 1)
#define SMP_CPU_FLAGS_WAIT              (1 << 2)
#define SMP_CPU_FLAGS_SLEEP             (1 << 3)
#define SMP_CPU_FLAGS_INTERRUPT         (1 << 4)


#define current_cpu     \
    (smp_get_current_cpu())

#define current_task    \
    (smp_get_current_cpu())->running_task




typedef struct {

    uint64_t id;
    uint64_t flags;
    uint64_t features[SMP_CPU_MAX_FEATURES];
    
    vmm_address_space_t address_space;

    task_t*  running_task;
    uint64_t running_ticks;

    struct timespec uptime;
    int errno;

} cpu_t;



__BEGIN_DECLS


cpu_t* smp_get_current_cpu(void);
cpu_t* smp_get_cpu(int);

void smp_init();

__END_DECLS

#endif
#endif