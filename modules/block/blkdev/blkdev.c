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
static list(blkdev_t*, ll_blkdev);


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
        return -1;
    }

    list_each(ll_blkdev, v) {
        if(strcmp(v->dev->name, name) == 0) {
            errno = EEXIST;
            return -1;
        }
    }


    blk->dev = vfs_mkdev(name, idx, S_IFBLK | blk->mode);
    blk->dev->userdata = (void*) blk;
    blk->dev->size = blk->blksize * blk->blkcount;
    blk->dev->read = blkdev_read;
    
    if(!(flags & BLKDEV_FLAGS_RDONLY))
        blk->dev->write = blkdev_write;

    if(flags & BLKDEV_FLAGS_MBR)
        blkdev_init_mbr(blk);


    kprintf(INFO "blkdev: registered \'%s\' (%d MB)\n", blk->dev->name, blk->dev->size >> 20);        
    list_push(ll_blkdev, blk);
    return 0;
}

int blkdev_unregister_device(char* name) {
    if(!name || strlen(name) == 0) {
        errno = EINVAL;
        return -1;
    }

    blkdev_t* blk = NULL;
    list_each(ll_blkdev, v) {
        if(strcmp(v->dev->name, name) != 0)
            continue;

        blk = v;
        break;
    }

    if(unlikely(!blk)) {
        errno = ENOENT;
        return -1;
    }

    list_remove(ll_blkdev, blk);


    blk->dev->read = NULL;
    blk->dev->write = NULL;
 

    kprintf(INFO "blkdev: unregistered \'%s\' (%d MB)\n", blk->dev->name, blk->dev->size >> 20);        
    vfs_unlink(blk->dev->parent, blk->dev->name);
    return 0;
}


int blkdev_read(inode_t* ino, void* buf, off_t pos, size_t size) {
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

    if(unlikely(pos > ino->size))
        return 0;

    if(unlikely((pos + size) > ino->size))
        size = ino->size - pos;

    if(unlikely(!size))
        return 0;


    uint32_t sb = pos / blkdev->blksize;
    uint32_t eb = (pos + size - 1) / blkdev->blksize;
    off64_t xoff = 0;


    if(pos % blkdev->blksize) {
        long p;
        p = blkdev->blksize - (pos % blkdev->blksize);
        p = p > size ? size : p;

        
        if(unlikely(!__cache_is_cached(blkdev->cache, sb))) {
            if(blkdev->read(blkdev->userdata, sb, blkdev->cache.c_data, 1) <= 0) {
                errno = EIO;
                return xoff;
            }

            __cache_update(blkdev->cache, sb, 1);
        }

        memcpy(buf, (void*) ((uintptr_t) blkdev->cache.c_data + ((uintptr_t) pos % blkdev->blksize)), p);

        xoff += p;
        sb++;
    }


    if(((pos + size) % blkdev->blksize) && (sb <= eb)) {
        long p = (pos + size) % blkdev->blksize;

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

    return size;
}

int blkdev_write(inode_t* ino, void* buf, off_t pos, size_t size) {
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


    if(unlikely(pos > ino->size))
        return 0;

    if(unlikely((pos + size) > ino->size))
        size = ino->size - pos;

    if(unlikely(!size))
        return 0;


    uint32_t sb = pos / blkdev->blksize;
    uint32_t eb = (pos + size - 1) / blkdev->blksize;
    off64_t xoff = 0;


    if(pos % blkdev->blksize) {
        long p;
        p = blkdev->blksize - (pos % blkdev->blksize);
        p = p > size ? size : p;

        
        if(unlikely(!__cache_is_cached(blkdev->cache, sb))) {
            if(blkdev->read(blkdev->userdata, sb, blkdev->cache.c_data, 1) <= 0) {
                errno = EIO;
                return xoff;
            }

            __cache_update(blkdev->cache, sb, 1);
        }


        memcpy((void*) ((uintptr_t) blkdev->cache.c_data + ((uintptr_t) pos % blkdev->blksize)), buf, p);
        
        if(blkdev->write(blkdev->userdata, sb, blkdev->cache.c_data, 1) <= 0) {
            errno = EIO;
            return xoff;
        }

        xoff += p;
        sb++;
    }


    if(((pos + size) % blkdev->blksize) && (sb <= eb)) {
        long p = (pos + size) % blkdev->blksize;

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

    return size;
}


int init(void) {
    memset(&ll_blkdev, 0, sizeof(ll_blkdev));

    return 0;
}

int dnit(void) { 
    list_each(ll_blkdev, v)
        blkdev_unregister_device(v->dev->name);

    return 0;
}