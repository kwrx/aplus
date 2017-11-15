#include <aplus.h>
#include <aplus/base.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <libc.h>

#if CONFIG_IOSCHED
int iosched_blksiz = 4096;

static void iosched_rw(inode_t* inode) {
    spinlock_lock(&inode->iolock);
    
    if(list_length(inode->ioqueue) == 0)
        goto done;

        
    struct iorequest* io = list_front(inode->ioqueue);
    list_pop_front(inode->ioqueue);
    
    int e = io->fn (
        io->inode, 
        (void*) ((uintptr_t) io->buffer + io->offset),
        io->position + io->offset, 
        io->size > iosched_blksiz
            ? iosched_blksiz
            : io->size
        );
        
    if(unlikely(e <= 0)) {
        if(unlikely(e < 0))
            io->ioerr = errno;

        spinlock_unlock(&io->lock);
        goto done;
    }

    io->offset += e;

    if(e < (io->size > iosched_blksiz
            ? iosched_blksiz
            : io->size)) {

                
        spinlock_unlock(&io->lock);
        goto done;
    }


    if(io->offset < io->size)
        list_push(inode->ioqueue, io);
    else
        spinlock_unlock(&io->lock);

done:
    spinlock_unlock(&inode->iolock);
}



int iosched_read(inode_t* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!buf || !size || !inode || !inode->read)) {
        errno = EINVAL;
        return -1;
    }

    struct iorequest* io = (struct iorequest*) kmalloc(sizeof(struct iorequest), GFP_KERNEL);
    io->inode = inode;
    io->buffer = buf;
    io->size = size;
    io->position = pos;
    io->offset = 0;
    io->ioerr = 0;
    io->fn = inode->read;

    spinlock_init(&io->lock);
    spinlock_lock(&io->lock);

    list_push(inode->ioqueue, io);
    
    while(spinlock_trylock(&io->lock) != E_OK)
        iosched_rw(inode);


    off_t off = io->offset;
    
    if(unlikely(io->ioerr != 0)) {
        errno = io->ioerr;
        off = -1;
    }

    kfree(io);
    return off;
}

int iosched_write(inode_t* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!buf || !size || !inode || !inode->write)) {
        errno = EINVAL;
        return -1;
    }

    struct iorequest* io = (struct iorequest*) kmalloc(sizeof(struct iorequest), GFP_KERNEL);
    io->inode = inode;
    io->buffer = buf;
    io->size = size;
    io->position = pos;
    io->offset = 0;
    io->ioerr = 0;
    io->fn = inode->write;

    spinlock_init(&io->lock);
    spinlock_lock(&io->lock);

    list_push(inode->ioqueue, io);
    
    while(spinlock_trylock(&io->lock) != E_OK)
        iosched_rw(inode);


    off_t off = io->offset;
    
    if(unlikely(io->ioerr != 0)) {
        errno = io->ioerr;
        off = -1;
    }

    kfree(io);
    return off;
}

int iosched_init(void) {
    //iosched_blksiz = sysconfig("iosched.blksize", SYSCONFIG_FORMAT_INT, 4096);
    return E_OK;
}
#endif