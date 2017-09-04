#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"



int fat_read(inode_t* ino, void* buf, size_t size) {
    if(unlikely(!ino))
        return 0;
        
    if(unlikely(!buf))
        return 0;
        
    if(unlikely((off64_t) size > ino->size))
        size = ino->size;
        
    if(unlikely(ino->position > ino->size))
        ino->position = ino->size;
        
    if(unlikely(ino->position + (off64_t) size > ino->size))
        size = (off_t) (ino->size - ino->position);
        
    if(unlikely(!size))
        return 0;

    fat_t* fat = (fat_t*) ino->userdata;
    zero_if(!fat, EINVAL);

    if(fat->entry_cluster == 0)
        return 0;
    
    int current_cluster = fat->entry_cluster;


    long i, s, c;
    for(i = 0; i < (ino->position / (fat->sector_per_cluster * fat->bytes_per_sector)); i++) {
        current_cluster = fatutils_next_cluster(fat, current_cluster);
        switch(current_cluster) {
            case FAT_BAD_CLUSTER:
            case FAT_UNUSED_CLUSTER:
            case FAT_END_CLUSTER:
                errno = EIO;
                return 0;
            default:
                continue;
        }
    }


    s = size;
    i = 0;
    c = fat->sector_per_cluster * fat->bytes_per_sector;

    
    do {
        char* p = (char*) buf; 
        fat->dev->position = (off64_t)((CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector) +
                                ((int)ino->position % (fat->sector_per_cluster * fat->bytes_per_sector)));

        if(i + c < s) {
            zero_if(vfs_read(fat->dev, &p[i], c) != c, EIO);
            i += c;
        } else {
            if(s - i > 0)
                zero_if(vfs_read(fat->dev, &p[i], s - i) != s - i, EIO);

            ino->position += (off64_t) size;
            return size;
        }
        

        current_cluster = fatutils_next_cluster(fat, current_cluster);
        switch(current_cluster) {
            case FAT_BAD_CLUSTER:
            case FAT_UNUSED_CLUSTER:
            case FAT_END_CLUSTER:
                errno = EIO;
                return 0;
            default:
                continue;
        }
    } while(1);

    errno = EIO;
    return 0;
}