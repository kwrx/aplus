#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_io_init(procfs_entry_t* e) {
    return 0;
}


int procfs_io_update(procfs_entry_t* e) {
    volatile task_t* tk = e->task
                            ? e->task : current_task;

    sprintf(e->data,
        "rchar: %lu\n"
        "wchar: %lu\n"
        "syscr: %lu\n"
        "syscw: %lu\n"
        "read_bytes: %lu\n"
        "write_bytes: %lu\n"
        "cancelled_write_bytes: %lu\n",
        (unsigned long) tk->iostat.rchar,
        (unsigned long) tk->iostat.wchar,
        (unsigned long) tk->iostat.syscr,
        (unsigned long) tk->iostat.syscw,
        (unsigned long) tk->iostat.read_bytes,
        (unsigned long) tk->iostat.write_bytes,
        (unsigned long) tk->iostat.cancelled_write_bytes
    );

    e->size = strlen(e->data);
    return 0;
}