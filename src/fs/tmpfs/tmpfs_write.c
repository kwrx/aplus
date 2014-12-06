#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>


static void tmpfs_realloc(inode_t* ino, int size) {
	ino->userdata = krealloc(ino->userdata, size);
	ino->size = size;
}


int tmpfs_write(inode_t* ino, char* buf, int size) {
	if(!ino)
		return 0;
		
	if(!buf)
		return 0;
			
	if(!size)
		return 0;
	
	if(ino->position + size > ino->size)
		tmpfs_realloc(ino, ino->position + size);

	if(!ino->userdata)
		return 0;


	memcpy(ino->userdata, buf, size);
	return size;
}
