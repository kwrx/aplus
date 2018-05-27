#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "iso9660.h"

MODULE_NAME("fs/iso9660");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




int init(void) {
    if(vfs_fsys_register(ISOFS_SUPER_MAGIC, "iso9660", iso9660_mount) != 0)
        return -1;

    return 0;
}



int dnit(void) {
    return 0;
}
