#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "iso9660.h"

int iso9660_write(inode_t* ino, void* buf, off_t pos, size_t size) {
    (void) ino;
    (void) buf;
    (void) pos;
    (void) size;
    
    errno = EROFS;
    return 0;
}
