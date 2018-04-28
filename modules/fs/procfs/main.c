#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"

MODULE_NAME("fs/procfs");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




int init(void) {
    if(vfs_fsys_register("procfs", procfs_mount) != E_OK)
        return -1;

    return 0;
}



int dnit(void) {
    return 0;
}
