#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"



inode_t* fat12_16_root_mknod(struct inode* inode, char* name, mode_t mode) {
    fat_t* fat = (fat_t*) inode->userdata;
    null_if(!fat, EINVAL);

    if(fat->entry_sector == 0)
        return NULL;

    fat->dev->position = fat->entry_sector * fat->bytes_per_sector;


    static char fat12_16_root_mknod_cache[8192];
    char* bufname = NULL;
    int e = FAT_EXTRACT_DONE;


    

    int i, j, k;
    for(j = 0; j < fat->rootdir_sectors * fat->bytes_per_sector; j += fat->bytes_per_sector) {
        null_if(vfs_read(fat->dev, fat12_16_root_mknod_cache, fat->bytes_per_sector) != fat->bytes_per_sector, EIO);

        for(i = 0; i < fat->bytes_per_sector; i += 32) {
            fat_entry_t* e = (fat_entry_t*) &fat12_16_root_mknod_cache[i];
            if(e->flags == ATTR_LFN)
                continue;
            if(e->name[0] != '\0')
                continue;


            memset(e, 0, sizeof(e));
            memset(e->name, ' ', 8);
            memcpy(e->name, name, strlen(name));

            if(S_ISDIR(mode))
                e->flags = ATTR_DIRECTORY;


            fat->dev->position = (uint32_t) (fat->entry_sector * fat->bytes_per_sector + j + i);
            null_if(vfs_write(fat->dev, e, sizeof(e)) != sizeof(e), EIO);

            inode_t* child;
            fatutils_new_child(fat, e, (uint32_t) (fat->entry_sector * fat->bytes_per_sector + j + i), &child, inode);
            null_if(!child, ENOMEM);

            child->name = strdup(name);
            return child;
        }
    }

    errno = ENOSPC;
    return NULL;
}