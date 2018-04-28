#include <aplus.h>
#include <aplus/module.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

MODULE_NAME("char/zero");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int zero_read(struct inode* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!buf || !size)) {
        errno = EINVAL;
        return -1;
    }
    
    memset(buf, 0, size);
    return size;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("zero", -1, S_IFCHR | 0444)) == NULL))
        return -1;


    ino->read = zero_read;
    return 0;
}



int dnit(void) {
    sys_unlink("/dev/zero");
    return 0;
}
