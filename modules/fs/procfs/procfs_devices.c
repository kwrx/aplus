#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_devices_init(procfs_entry_t* e) {
    return 0;
}


int procfs_devices_update(procfs_entry_t* e) {

    memset(e->data, 0, strlen(e->data));
    strcat(e->data, "Character devices:\n");

    struct inode_childs* cx;
    for(cx = devfs->childs; cx; cx = cx->next) {
        if(!S_ISCHR(cx->inode->mode))
            continue;

        char buf[64];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "%3d %s\n", cx->inode->dev, cx->inode->name);
        strcat(e->data, buf);
    }


    strcat(e->data, "\nBlock devices:\n");

    for(cx = devfs->childs; cx; cx = cx->next) {
        if(!S_ISBLK(cx->inode->mode))
            continue;

        char buf[64];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "%3d %s\n", cx->inode->dev, cx->inode->name);
        strcat(e->data, buf);
    }

    e->size = strlen(e->data);
    return 0;
}