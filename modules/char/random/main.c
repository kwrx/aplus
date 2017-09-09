#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <libc.h>

MODULE_NAME("char/random");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static mutex_t rnd_lock = MTX_INIT(MTX_KIND_DEFAULT, "random");

static int random_read(struct inode* inode, void* buf, size_t size) {
    mutex_lock(&rnd_lock);
    srand(sys_times(NULL));

    char* bc = (char*) buf;
    size_t i;
    for(i = 0; i < size; i++)
        *bc++ = rand() % 256;

    mutex_unlock(&rnd_lock);
    return size;
}

int init(void) {
    inode_t* ino;
    if(unlikely((ino = vfs_mkdev("random", -1, S_IFCHR | 0444)) == NULL))
        return E_ERR;


    ino->read = random_read;
    return E_OK;
}



int dnit(void) {
    sys_unlink("/dev/random");
    return E_OK;
}
