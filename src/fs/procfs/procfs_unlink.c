#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

int procfs_unlink(inode_t* inode, char* name) {
	if((void*) vfs_mapped(inode, name) != NULL)
		return -1;
		
	return vfs_umap(inode, name);
}
