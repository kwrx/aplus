#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <xdev/task.h>
#include <xdev/timer.h>
#include <libc.h>

#include "iso9660.h"





int iso9660_unlink(struct inode* inode, char* path) {
    (void) inode;
    (void) path;
    
    errno = EROFS;
    return E_ERR;
}