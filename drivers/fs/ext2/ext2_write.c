#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "ext2.h"



int ext2_write(inode_t* ino, void* buf, off_t pos, size_t size) {
    (void) ino;
    (void) buf;
    (void) pos;
    (void) size;
    
    errno = EROFS;
    return 0;
}