#ifndef _PROCFS_H
#define _PROCFS_H

#include <aplus.h>
#include <aplus/base.h>
#include <aplus/task.h>
#include <aplus/debug.h>

typedef struct procfs {
    task_t* tk;
    void* data;
} procfs_t;


static inline void* procfs_make_userdata(task_t* tk, void* data) {
    procfs_t* pfs = (procfs_t*) kmalloc(sizeof(procfs_t), GFP_KERNEL);
    if(unlikely(!pfs)) {
        kprintf(ERROR "procfs: no memory left!\n");
        return NULL;
    }

    pfs->tk = tk;
    pfs->data = data;

    return (void*) pfs;
}

int procfs_mount(struct inode* dev, struct inode* dir);
int procfs_read(struct inode* inode, void* ptr, off_t pos, size_t len);
int procfs_add_childs(inode_t* parent, task_t* tk);

#endif
