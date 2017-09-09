#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/blkdev.h>
#include <aplus/utils/hashmap.h>
#include <libc.h>

MODULE_NAME("block/blkdev");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


extern int blkdev_init_mbr(blkdev_t* blkdev);
static hashmap_t hm_blkdev;


#define __cache_is_cached(x, y)                 \
    (x.c_cached && (x.c_blkno == y))

#define __cache_update(x, y, z)                 \
    {                                           \
        x.c_blkno = y;                          \
        x.c_cached = z;                         \
    }


int blkdev_register_device(blkdev_t* blk, char* name, int idx, int flags) {
    if(!blk || !name) {
        errno = EINVAL;
        return E_ERR;
    }

    if(hashmap_get(hm_blkdev, name, NULL) == HM_OK) {
        errno = EEXIST;
        return E_ERR;
    }


    blk->dev = vfs_mkdev(name, idx, S_IFBLK | blk->mode);
    blk->dev->userdata = (void*) blk;
    blk->dev->size = blk->blksize * blk->blkcount;
    blk->dev->read = blkdev_read;
    
    if(!(flags & BLKDEV_FLAGS_RDONLY))
        blk->dev->write = blkdev_write;

    if(flags & BLKDEV_FLAGS_MBR)
        blkdev_init_mbr(blk);


    hashmap_put(hm_blkdev, name, blk);
    kprintf(INFO "blkdev: registered \'%s\'\n", blk->dev->name);
    return E_OK;
}

int blkdev_unregister_device(char* name) {
    if(!name || strlen(name) == 0) {
        errno = EINVAL;
        return E_ERR;
    }

    blkdev_t* blk;
    if(hashmap_get(hm_blkdev, name, (any_t) &blk) != HM_OK) {
        errno = ESRCH;
        return E_ERR;
    }
    
    hashmap_remove(hm_blkdev, name);


    blk->dev->read = NULL;
    blk->dev->write = NULL;
    
    vfs_unlink(blk->dev->parent, blk->dev->name);
    return E_OK;
}


int blkdev_read(inode_t* ino, void* buf, size_t size) {
    if(unlikely(!ino || !buf)) {
        errno = EINVAL;
        return -1;
    }

    blkdev_t* blkdev = (blkdev_t*) ino->userdata;
    if(unlikely(!blkdev)) {
        errno = ENODEV;
        return -1;
    }


    if(unlikely(!blkdev->read))
        return 0;

    if(unlikely(ino->position > ino->size))
        return 0;

    if(unlikely((ino->position + size) > ino->size))
        size = ino->size - ino->position;

    if(unlikely(!size))
        return 0;


    uint32_t sb = ino->position / blkdev->blksize;
    uint32_t eb = (ino->position + size - 1) / blkdev->blksize;
    off64_t xoff = 0;


    if(ino->position % blkdev->blksize) {
        long p;
        p = blkdev->blksize - (ino->position % blkdev->blksize);
        p = p > size ? size : p;

        
        if(unlikely(!__cache_is_cached(blkdev->cache, sb))) {
            if(blkdev->read(blkdev->userdata, sb, blkdev->cache.c_data, 1) <= 0) {
                errno = EIO;
                return xoff;
            }

            __cache_update(blkdev->cache, sb, 1);
        }

        memcpy(buf, (void*) ((uintptr_t) blkdev->cache.c_data + ((uintptr_t) ino->position % blkdev->blksize)), p);

        xoff += p;
        sb++;
    }


    if(((ino->position + size) % blkdev->blksize) && (sb <= eb)) {
        long p = (ino->position + size) % blkdev->blksize;

        if(unlikely(!__cache_is_cached(blkdev->cache, eb))) {
            if(blkdev->read(blkdev->userdata, eb, blkdev->cache.c_data, 1) <= 0) {
                errno = EIO;
                return xoff;
            }

            __cache_update(blkdev->cache, eb, 1);
        }

        memcpy((void*) ((uintptr_t) buf + size - p), blkdev->cache.c_data, p);
        eb--;
    }


    long i = eb - sb + 1;
    if(likely(i > 0)) {
        if(unlikely(blkdev->read(blkdev->userdata, sb, (void*) ((uintptr_t) buf + (uintptr_t) xoff), i)) <= 0) {
            errno = EIO;
            return xoff;
        }
    }

    ino->position += size;
    return size;
}

int blkdev_write(inode_t* ino, void* buf, size_t size) {
    if(unlikely(!ino || !buf)) {
        errno = EINVAL;
        return -1;
    }

    blkdev_t* blkdev = (blkdev_t*) ino->userdata;
    if(unlikely(!blkdev)) {
        errno = ENODEV;
        return -1;
    }

    if(unlikely(!blkdev->write))
        return 0;


    if(unlikely(ino->position > ino->size))
        return 0;

    if(unlikely((ino->position + size) > ino->size))
        size = ino->size - ino->position;

    if(unlikely(!size))
        return 0;


    uint32_t sb = ino->position / blkdev->blksize;
    uint32_t eb = (ino->position + size - 1) / blkdev->blksize;
    off64_t xoff = 0;


    if(ino->position % blkdev->blksize) {
        long p;
        p = blkdev->blksize - (ino->position % blkdev->blksize);
        p = p > size ? size : p;

        
        if(unlikely(!__cache_is_cached(blkdev->cache, sb))) {
            if(blkdev->read(blkdev->userdata, sb, blkdev->cache.c_data, 1) <= 0) {
                errno = EIO;
                return xoff;
            }

            __cache_update(blkdev->cache, sb, 1);
        }


        memcpy((void*) ((uintptr_t) blkdev->cache.c_data + ((uintptr_t) ino->position % blkdev->blksize)), buf, p);
        
        if(blkdev->write(blkdev->userdata, sb, blkdev->cache.c_data, 1) <= 0) {
            errno = EIO;
            return xoff;
        }

        xoff += p;
        sb++;
    }


    if(((ino->position + size) % blkdev->blksize) && (sb <= eb)) {
        long p = (ino->position + size) % blkdev->blksize;

        if(unlikely(!__cache_is_cached(blkdev->cache, eb))) {
            if(blkdev->read(blkdev->userdata, eb, blkdev->cache.c_data, 1) <= 0) {
                errno = EIO;
                return xoff;
            }

            __cache_update(blkdev->cache, eb, 1);
        }


        memcpy(blkdev->cache.c_data, (void*) ((uintptr_t) buf + size - p), p);

        if(blkdev->write(blkdev->userdata, eb, blkdev->cache.c_data, 1) <= 0) {
            errno = EIO;
            return xoff;
        }

        eb--;
    }

    long i = eb - sb + 1;
    if(likely(i > 0)) {
        if(unlikely(blkdev->write(blkdev->userdata, sb, (void*) ((uintptr_t) buf + (uintptr_t) xoff), i)) <= 0) {
            errno = EIO;
            return xoff;
        }
    }

    ino->position += size;
    return size;
}


int init(void) {
    hm_blkdev = hashmap_new();
    if(!hm_blkdev) {
        kprintf(ERROR "blkdev: could not allocate memory!\n");
        return E_ERR;
    }


    return E_OK;
}

int dnit(void) {
#if 0  
    blkdev_t* blk;
    while(hashmap_get_one(hm_blkdev, (any_t*) &blk, 1) == HM_OK)
        vfs_unlink(blk->dev->parent, blk->dev->name);
#endif

    hashmap_free(hm_blkdev);
    return E_OK;
}