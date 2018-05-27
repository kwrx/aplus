#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "fat.h"


int fat_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {
    if(unlikely(!dev || !dir)) {
        errno = EINVAL;
        return -1;
    }

    fat_superblock_t sb;
    if(vfs_read(dev, &sb, 0, sizeof(sb)) != sizeof(sb)) {
        errno = EIO;
        return -1;
    }

    
}