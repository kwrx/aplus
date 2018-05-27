#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "fat.h"

MODULE_NAME("fs/fat");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




int init(void) {
    if(vfs_fsys_register(MSDOS_SUPER_MAGIC, "fat", fat_mount) != 0)
        return -1;

    return 0;
}



int dnit(void) {
    return 0;
}
