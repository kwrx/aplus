#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "iso9660.h"





int iso9660_unlink(struct inode* inode, char* path) {
    (void) inode;
    (void) path;
    
    errno = EROFS;
    return -1;
}