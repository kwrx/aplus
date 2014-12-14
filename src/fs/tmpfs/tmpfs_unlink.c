#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

int tmpfs_unlink(inode_t* inode, char* name) {
	inode_t* ino;
	if((ino = (inode_t*) vfs_mapped(inode, name)) != NULL)
		return -1;
		
	return vfs_umap(ino);
}
