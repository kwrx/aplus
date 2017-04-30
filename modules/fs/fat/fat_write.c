#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"



int fat_write(inode_t* ino, void* buf, size_t size) {
    if(unlikely(!ino))
		return 0;

	if(unlikely(!buf))
		return 0;
		
	if(unlikely(!size))
		return 0;

    if(ino->position > ino->size)
        return 0;


	fat_t* fat = (fat_t*) ino->userdata;
    zero_if(!fat, EINVAL);


	
	int current_cluster = fat->entry_cluster;
    int end_cluster = 0;
    

    errno = 0;

    int i, s, c;
    for(i = 0; i < (ino->position / (fat->sector_per_cluster * fat->bytes_per_sector)); i++) {
        current_cluster = fatutils_next_cluster(fat, current_cluster);
        switch(current_cluster) {
            case FAT_BAD_CLUSTER:
            case FAT_UNUSED_CLUSTER:
                errno = EIO;
            case FAT_END_CLUSTER:
                end_cluster = 1;
                break;
            default:
                continue;
        }
    }

    if(errno == EIO)
        return 0;


    s = size;
    i = 0;
    c = fat->sector_per_cluster * fat->bytes_per_sector;



    if(end_cluster)
        if(fatutils_alloc_cluster(fat, current_cluster, &current_cluster) == E_ERR)
            return 0;

    if(!fat->entry_cluster)
        fat->entry_cluster = current_cluster;
    
    do {
        char* p = (char*) buf; 
        fat->dev->position = (off64_t)((CLUSTER_TO_SECTOR(fat, current_cluster) * fat->bytes_per_sector) +
                                ((int)ino->position % (fat->sector_per_cluster * fat->bytes_per_sector)));
        
        if(i + c < s) {
            zero_if(vfs_write(fat->dev, &p[i], c) != c, EIO);
            i += c;
        } else {
            if(s - i > 0)
                zero_if(vfs_write(fat->dev, &p[i], s - i) != s - i, EIO);
            

            ino->position += (off64_t) size;
            ino->size = ino->position > ino->size ? ino->position : ino->size;
            
            if(unlikely(fatutils_update_entry(fat, ino, 0) == E_ERR))
                kprintf(WARN "[FAT] Warning: cannot update entry for %s\n", ino->name);
                
            if(unlikely(fatutils_update_fat(fat) == E_ERR))
                kprintf(WARN "[FAT] Warning: cannot update FAT for %s\n", ino->name);

            return size;
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

	errno = EIO;
	return 0;
}