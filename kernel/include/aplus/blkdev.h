#ifndef _BLKDEV_H
#define _BLKDEV_H

#include <aplus.h>
#include <aplus/base.h>

#define BLKDEV_BLKMAXSIZE           0x1000

#define BLKDEV_FLAGS_RDONLY         1
#define BLKDEV_FLAGS_MBR            2



typedef struct {
    inode_t* dev;
    mode_t mode;
    size_t blksize;
    size_t blkcount;

    size_t (*read) (void* userdata, uint32_t blkno, void* buf, size_t count);
    size_t (*write) (void* userdata, uint32_t blkno, void* buf, size_t count);

    struct {
        char c_data[BLKDEV_BLKMAXSIZE];
        uint64_t c_blkno;
        int c_cached;
    } cache;

    void* userdata;
} blkdev_t;


int blkdev_register_device(blkdev_t* blk, char* name, int idx, int flags);
int blkdev_unregister_device(char* name);
int blkdev_read(inode_t* ino, void* buf, size_t size);
int blkdev_write(inode_t* inode, void* buf, size_t size);


#endif