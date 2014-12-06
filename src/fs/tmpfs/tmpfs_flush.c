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


void tmpfs_flush(inode_t* ino) {
	if(ino->userdata)
		kfree(ino->userdata);

	ino->size = 0;
}
