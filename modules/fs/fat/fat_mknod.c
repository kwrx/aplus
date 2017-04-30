#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"



inode_t* fat_mknod(struct inode* inode, char* name, mode_t mode) {
    fat_t* fat = (fat_t*) inode->userdata;
    null_if(!fat, EINVAL);

    if(fat->entry_cluster == 0)
        null_if(fatutils_alloc_cluster(fat, -1, &fat->entry_cluster) == E_ERR, ENOSPC);
       

    static char fat_mknod_cache[8192];
    char* bufname = NULL;
    int e = FAT_EXTRACT_DONE;

    int current_cluster = fat->entry_cluster;


    do { 
        fat->dev->position = CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector;
        
        int i, j;
        for(j = 0; j < fat->sector_per_cluster * fat->bytes_per_sector; j += fat->bytes_per_sector) {
            null_if(vfs_read(fat->dev, fat_mknod_cache, fat->bytes_per_sector) != fat->bytes_per_sector, EIO);

            for(i = 0; i < fat->bytes_per_sector; i += 32) {
                fat_entry_t* e = (fat_entry_t*) &fat_mknod_cache[i];
                if(e->flags == ATTR_LFN)
                    continue;
                if(e->name[0] != '\0')
                    continue;


                memset(e, 0, sizeof(e));
                memset(e->name, ' ', 8);
                memcpy(e->name, name, strlen(name));

                if(S_ISDIR(mode))
                    e->flags = ATTR_DIRECTORY;


                fat->dev->position = (uint32_t) (CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector + j + i);
                null_if(vfs_write(fat->dev, e, sizeof(e)) != sizeof(e), EIO);
                null_if(fatutils_update_fat(fat) == E_ERR, EIO);

                inode_t* child;
                fatutils_new_child(fat, e, (uint32_t) (CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector + j + i), &child, inode);
                null_if(!child, ENOMEM);

                child->name = strdup(name);
                return child;
            }
        }

        int ncluster = fatutils_next_cluster(fat, current_cluster);
        switch(ncluster) {
            case FAT_BAD_CLUSTER:
            case FAT_UNUSED_CLUSTER:
                errno = EIO;
            case FAT_END_CLUSTER:
                if(fatutils_alloc_cluster(fat, current_cluster, &current_cluster) == E_ERR)
                    return 0;
                continue;
            default:
                current_cluster = ncluster;
                continue;
        }
    } while(1);

    errno = ENOSPC;
    return NULL;
}