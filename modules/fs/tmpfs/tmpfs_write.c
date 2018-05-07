#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "tmpfs.h"


int tmpfs_write(struct inode* inode, void* ptr, off_t pos, size_t len) {
    if(unlikely(!inode))
        return 0;

    if(unlikely(!ptr))
        return 0;

    if(unlikely(!len))
        return 0;

    if(pos + len > inode->size) {
        if(unlikely(
            (
                (long) inode->mtinfo->stat.f_bfree - (long) ((pos + len) - inode->size)
            ) <= 0L)
        ) {
            errno = ENOSPC;
            return 0;
        }

        void* np = (void*) kmalloc(pos + len, GFP_USER);
        
        if(likely(inode->userdata)) {
            memcpy(np, inode->userdata, inode->size);        
            kfree(inode->userdata);
        }



        inode->mtinfo->stat.f_bfree -= (pos + len) - inode->size;
        inode->mtinfo->stat.f_bavail -= (pos + len) - inode->size;

        inode->size = pos + (off64_t) len;
        inode->userdata = np;
    }

    memcpy((void*) ((uintptr_t) inode->userdata + (uintptr_t) pos), ptr, len);
    return len;
}