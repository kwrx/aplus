#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"


inode_t* fat_finddir(struct inode* inode, char* name) { 
    fat_t* fat = (fat_t*) inode->userdata;
    null_if(!fat, EINVAL);

    if(fat->entry_cluster == 0)
        return NULL;


    static char fat_finddir_cache[8192];
    char* bufname = NULL;
    int e = FAT_EXTRACT_DONE;

    int current_cluster = fat->entry_cluster;


    do { 
        fat->dev->position = CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector;
        
        int i, j;
        for(j = 0; j < fat->sector_per_cluster * fat->bytes_per_sector; j += fat->bytes_per_sector) {
            null_if(vfs_read(fat->dev, fat_finddir_cache, fat->bytes_per_sector) != fat->bytes_per_sector, EIO);

            for(i = 0; i < fat->bytes_per_sector; i += 32) {
                bufname = fatutils_extract_name(bufname, &e, (fat_entry_t*) &fat_finddir_cache[i]);

                switch(e) {
                    case FAT_EXTRACT_END:
                        return E_OK;
                    case FAT_EXTRACT_CONTINUE:
                        continue;
                    case FAT_EXTRACT_DONE: {
                        null_if(!bufname, EINVAL);

                        if(strcmp(name, bufname) != 0) {
                            kfree(bufname);
                            bufname = NULL;
                            break;
                        }

                        inode_t* child;
                        fatutils_new_child(fat, (fat_entry_t*) &fat_finddir_cache[i], (uint32_t) (CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector + j + i), &child, inode);
                        null_if(!child, ENOMEM);

                        child->name = bufname;
                        return child;
                    } break;
                }
            }
        }

        current_cluster = fatutils_next_cluster(fat, current_cluster);
        switch(current_cluster) {
            case FAT_BAD_CLUSTER:
            case FAT_UNUSED_CLUSTER:
            case FAT_END_CLUSTER:
                return E_OK;
            default:
                continue;
        }
    } while(1);

    return E_OK;
}