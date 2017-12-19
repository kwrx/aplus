#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>


MODULE_NAME("fs/devfs");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




static int devfs_mount(struct inode* dev, struct inode* dir) {
    (void) dev;

    if(!dir->parent) {
        errno = EINVAL;
        return -1;
    }



    extern inode_t* devfs;

    struct inode_childs* tmp;
    for(tmp = dir->parent->childs; tmp; tmp = tmp->next)
        if(tmp->inode == dir)
            break;

    if(tmp == NULL) {
        kprintf(ERROR "BUG: WTF! %s:%d %s()\n", __FILE__, __LINE__, __func__);

        errno = ENOENT;
        return -1;
    }

    devfs->parent = dir->parent;
    tmp->inode = devfs;
    return E_OK;
}


int init(void) {
    if(vfs_fsys_register("devfs", devfs_mount) != E_OK)
        return E_ERR;

    return E_OK;
}



int dnit(void) {
    return E_OK;
}
