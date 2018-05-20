#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_uptime_init(procfs_entry_t* e) {
    return 0;
}


int procfs_uptime_update(procfs_entry_t* e) {
    sprintf(e->data,
        "%.2f %.2f\n",
        (double) timer_getms() / 1000.0,
        0.0
    );

    e->size = strlen(e->data);
    return 0;
}