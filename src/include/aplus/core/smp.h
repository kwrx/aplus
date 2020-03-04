#ifndef _APLUS_CORE_SMP_H
#define _APLUS_CORE_SMP_H

#ifndef __ASSEMBLY__
#include <time.h>
#include <sys/cdefs.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>
#include <aplus/core/task.h>

#define SMP_CPU_MAX                     256
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
    uint64_t features;
    uint64_t xfeatures;
    vmm_address_space_t address_space;

    task_t* running_task;
    struct timespec ticks;

    int errno;

} cpu_t;

__BEGIN_DECLS


cpu_t* smp_get_current_cpu(void);
cpu_t* smp_get_cpu(int);

void smp_init();

__END_DECLS

#endif
#endif