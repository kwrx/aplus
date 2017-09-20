#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <libc.h>


MODULE_NAME("sys/log");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int log_write(struct inode* inode, void* buf, size_t size) {
#if DEBUG
    ((char*) buf) [size] = 0;
    //kprintf(LOG "%s", buf);
    for(char* p = buf; *p; p++)
        debug_send(*p);
#endif

    return size;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("log", -1, S_IFCHR | 0222)) == NULL))
        return E_ERR;


    ino->write = log_write;
    return E_OK;
}



int dnit(void) {
    return E_OK;
}
