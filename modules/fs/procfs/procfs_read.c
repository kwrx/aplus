#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_read(struct inode* inode, void* ptr, off_t pos, size_t len) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!ptr))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return 0;

    if(pos + len > inode->size)
        len = (size_t) inode->size - pos;

    if(unlikely(!len))
        return 0;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    if(unlikely(!pfs->data))
        return 0;


    memcpy(ptr, (void*) ((uintptr_t) pfs->data + (uintptr_t) pos), len);
    return len;
}
