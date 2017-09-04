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


static int zero_read(struct inode* inode, void* buf, size_t size) {
    memset(buf, 0, size);
    return size;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("zero", -1, S_IFCHR | 0444)) == NULL))
        return E_ERR;


    ino->read = zero_read;
    return E_OK;
}



int dnit(void) {
    return E_OK;
}
