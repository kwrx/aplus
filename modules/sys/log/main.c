#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <libc.h>


MODULE_NAME("sys/log");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static int log_write(struct inode* inode, void* buf, off_t pos, size_t size) {
#if DEBUG
    ((char*) buf) [size] = 0;
    kprintf(LOG "");

    for(char* p = buf; *p; p++) {
        debug_send(*p);

        if(*p == '\n' && p[1] != '\0')
            kprintf(LOG "");
    }
#endif

    return size;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("log", -1, S_IFCHR | 0222)) == NULL))
        return -1;


    ino->write = log_write;
    return 0;
}



int dnit(void) {
    return 0;
}
