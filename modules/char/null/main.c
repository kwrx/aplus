#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <libc.h>

MODULE_NAME("char/null");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int null_read_write(struct inode* inode, void* buf, size_t size) {
    return size;
}


int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("null", -1, S_IFCHR | 0666)) == NULL))
        return E_ERR;

    ino->read =
    ino->write = null_read_write;

    return E_OK;
}



int dnit(void) {
    return E_OK;
}
