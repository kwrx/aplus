#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"


int fat_open(struct inode* inode) {
    fat_t* fat = (fat_t*) inode->userdata;
    fail_if(!fat, EINVAL);

    if(fat->entry_cluster == 0)
        return E_OK;


    static char fat_open_cache[8192];
    char* bufname = NULL;
    int e = FAT_EXTRACT_DONE;

    int current_cluster = fat->entry_cluster;

    
    do { 
        fat->dev->position = CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector;
        
        int i, j;
        for(j = 0; j < fat->sector_per_cluster * fat->bytes_per_sector; j += fat->bytes_per_sector) {
            fail_if(vfs_read(fat->dev, fat_open_cache, fat->bytes_per_sector) != fat->bytes_per_sector, EIO);

            for(i = 0; i < fat->bytes_per_sector; i += 32) {
                bufname = fatutils_extract_name(bufname, &e, (fat_entry_t*) &fat_open_cache[i]);

                switch(e) {
                    case FAT_EXTRACT_END:
                        return E_OK;
                    case FAT_EXTRACT_CONTINUE:
                        continue;
                    case FAT_EXTRACT_DONE: {
                        fail_if(!bufname, EINVAL);

                        if(bufname[0] == '.' && bufname[1] == '\0')
                            goto done;

                        if(bufname[0] == '.' && bufname[1] == '.' && bufname[2] == '\0')
                            goto done;

                        struct inode_childs* cx;
                        for(cx = inode->childs; cx; cx = cx->next) {
                            if(strcmp(cx->inode->name, bufname) == 0) {
                                kfree((void*) bufname);
                                bufname = NULL;
                                
                                goto done;
                            }
                        }


                        inode_t* child;
                        fatutils_new_child(fat, (fat_entry_t*) &fat_open_cache[i], (uint32_t) (CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector + j + i), &child, inode);
                        fail_if(!child, ENOMEM);

                        child->name = bufname;

                        cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
                        fail_if(!cx, ENOMEM);

                        cx->inode = child;
                        cx->next = inode->childs;
                        inode->childs = cx;

                        break;
done:
                        kfree(bufname);
                        bufname = NULL;
                        break;
                    }
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