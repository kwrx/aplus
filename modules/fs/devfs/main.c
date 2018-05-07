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




static int devfs_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {
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
    devfs->mtinfo = info;
    dir->mtinfo = info;
    tmp->inode = devfs;


    info->stat.f_bsize = PAGE_SIZE;
    info->stat.f_frsize = PAGE_SIZE;
    info->stat.f_blocks = 0x100000 / PAGE_SIZE;
    info->stat.f_bfree = 0x100000 / PAGE_SIZE;
    info->stat.f_bavail = 0x100000 / PAGE_SIZE;
    info->stat.f_files = 0x1000;
    info->stat.f_ffree = 0x1000;
    info->stat.f_favail = 0x1000;
    info->stat.f_namemax = BUFSIZ;

    return 0;
}


int init(void) {
    if(vfs_fsys_register(DEVPTS_SUPER_MAGIC, "devtmpfs", devfs_mount) != E_OK)
        return -1;

    return 0;
}



int dnit(void) {
    return 0;
}
