#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"




int fat_unlink(struct inode* inode, char* name) {
    inode_t* ino = vfs_finddir(inode, name);
    if(!ino)
        return E_ERR;

    fat_t* fat = (fat_t*) ino->userdata;
    fail_if(!fat, EINVAL);


    if(!fat->entry_cluster)
        goto done;


    int current_cluster = fat->entry_cluster;

    do {
        current_cluster = fatutils_free_cluster(fat, current_cluster);

        switch(current_cluster) {
            case FAT_BAD_CLUSTER:
            case FAT_UNUSED_CLUSTER:
            case FAT_END_CLUSTER:
                goto done;
            default:
                continue;
        }
    } while(1);



done:

    if(unlikely(fatutils_update_entry(fat, ino, 1) == E_ERR))
        kprintf(WARN "[FAT] Warning: cannot update entry for %s\n", ino->name);

    if(unlikely(fatutils_update_fat(fat) == E_ERR))
    kprintf(WARN "[FAT] Warning: cannot update FAT for %s\n", ino->name);


    kfree(fat);
    return E_OK;
}