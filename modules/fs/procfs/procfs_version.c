#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_version_init(procfs_entry_t* e) {
    sprintf(e->data,
        "%s version %s-%s (aplus-gcc %d.%d.%d) %s %s\n",
        KERNEL_NAME,
        KERNEL_VERSION, KERNEL_CODENAME,
        __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
        KERNEL_DATE,
        KERNEL_TIME
    );

    e->size = strlen(e->data);
    return 0;
}


int procfs_version_update(procfs_entry_t* e) {
    return 0;
}