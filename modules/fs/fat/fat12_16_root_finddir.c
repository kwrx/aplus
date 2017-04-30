#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"


inode_t* fat12_16_root_finddir(struct inode* inode, char* name) {
    fat_t* fat = (fat_t*) inode->userdata;
    null_if(!fat, EINVAL);

    if(fat->entry_sector == 0)
        return NULL;

    fat->dev->position = fat->entry_sector * fat->bytes_per_sector;


    static char fat12_16_root_finddir_cache[8192];
    char* bufname = NULL;
    int e = FAT_EXTRACT_DONE;

    int i, j;
    for(j = 0; j < fat->rootdir_sectors * fat->bytes_per_sector; j += fat->bytes_per_sector) {
        null_if(vfs_read(fat->dev, fat12_16_root_finddir_cache, fat->bytes_per_sector) != fat->bytes_per_sector, EIO);

        for(i = 0; i < fat->bytes_per_sector; i += 32) {
            bufname = fatutils_extract_name(bufname, &e, (fat_entry_t*) &fat12_16_root_finddir_cache[i]);

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
                    fatutils_new_child(fat, (fat_entry_t*) &fat12_16_root_finddir_cache[i], (uint32_t) (fat->entry_sector * fat->bytes_per_sector + j + i), &child, inode);
                    null_if(!child, ENOMEM);

                    child->name = bufname;
                    return child;
                } break;
            }
        }
    }

    return E_OK;
}