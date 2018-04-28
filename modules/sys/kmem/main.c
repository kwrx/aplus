#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include <aplus/kmem.h>

/* Global Shared Memory */

MODULE_NAME("sys/kmem");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");

static int kmem_ioctl(struct inode* inode, int req, void* ptr) {
    #define cp(x)                   \
        if(unlikely(!x)) {          \
            errno = EINVAL;         \
            return -1;           \
        }
    
    switch(req) {
        case KMEMIOCTL_ALLOC:
            cp(ptr);
            *((uintptr_t*) ptr) = (uintptr_t) kmalloc(*((uintptr_t*) ptr), GFP_USER);
            
            break;
        case KMEMIOCTL_FREE:
            cp(ptr);
            kfree(ptr);
                
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    
    return 0;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("kmem", -1, S_IFCHR | 0440)) == NULL))
        return -1;


    ino->ioctl = kmem_ioctl;
    return 0;
}



int dnit(void) {
    return 0;
}
