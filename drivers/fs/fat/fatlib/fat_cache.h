#ifndef __FAT_CACHE_H__
#define __FAT_CACHE_H__

#include "fat_filelib.h"


struct fatfs_cache {
    uint32 cluster_cache_idx[FAT_CLUSTER_CACHE_ENTRIES];
    uint32 cluster_cache_data[FAT_CLUSTER_CACHE_ENTRIES];
};

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
int fatfs_cache_init(struct fatfs *fs, struct fatfs_cache *file);
int fatfs_cache_get_next_cluster(struct fatfs *fs, struct fatfs_cache *file, uint32 clusterIdx, uint32 *pNextCluster);
int fatfs_cache_set_next_cluster(struct fatfs *fs, struct fatfs_cache *file, uint32 clusterIdx, uint32 nextCluster);

#endif
