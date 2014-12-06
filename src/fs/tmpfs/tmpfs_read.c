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



int tmpfs_read(inode_t* ino, char* buf, int size) {
	if(!ino)
		return 0;
		
	if(!buf)
		return 0;
		
	if(size > ino->size)
		size = ino->size;
		
	if(ino->position > ino->size)
		ino->position = ino->size;
		
	if(ino->position + size > ino->size)
		size = ino->size - ino->position;
		
	if(!size)
		return 0;

	if(!ino->userdata)
		return 0;


	memcpy(buf, ino->userdata, size);
	return size;
}
