#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_read(struct inode* inode, void* ptr, off_t pos, size_t len) {
    if(unlikely(!inode))
        return -1;

    if(unlikely(!ptr))
        return -1;

    if(unlikely(!inode->userdata))
        return 0;

    if(pos + len > inode->size)
        len = (size_t) inode->size - pos;

    if(unlikely(!len))
        return 0;


    procfs_entry_t* e = (procfs_entry_t*) inode->userdata;
    memcpy(ptr, &e->data[pos], len);
    
    return len;
}
