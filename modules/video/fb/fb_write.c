#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/debug.h>
#include <xdev/mm.h>
#include <libc.h>

#include <aplus/fbdev.h>

extern fbdev_t* fbdev;

int fb_write(struct inode* inode, void* ptr, size_t len) {
	
	int i;
	char* buf;
	for(buf = ptr, i = 0; i < len; i++)
		kprintf(USER, "%c", *buf++);
			
	
    return len;
}