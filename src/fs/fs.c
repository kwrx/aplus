#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/mm.h>

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>



int fs_read (struct inode* inode, char* ptr, int len) {
	if(likely(inode->read))
		return inode->read(inode, ptr, len);
	
	errno = ENOSYS;		
	return 0;
}

int fs_write (struct inode* inode, char* ptr, int len) {
	if(likely(inode->write))
		return inode->write(inode, ptr, len);
		
	errno = ENOSYS;	
	return 0;
}
	
struct dirent* fs_readdir (struct inode* inode, int index) {

	inode_t* map = NULL;
	if(likely((map = (inode_t*) vfs_mapped_at_index(inode, index)) != NULL)) {
		struct dirent* ent = (struct dirent*) kmalloc(sizeof(struct dirent));
		strcpy(ent->d_name, map->name);
		ent->d_ino = map->ino;
		
		return ent;
	}

	if(likely(inode->readdir))
		return inode->readdir(inode, index - vfs_mapped_count(inode));
		
	errno = ENOSYS;	
	return NULL;
}

struct inode* fs_finddir (struct inode* inode, char* name) {

	if(unlikely(strcmp(name, ".") == 0))
		return inode;

	if(unlikely(strcmp(name, "..") == 0))
		return inode->parent;

	inode_t* map = NULL;
	if(likely((map = (inode_t*) vfs_mapped(inode, name)) != NULL))
		return map;

	if(likely(inode->finddir))
		return inode->finddir(inode, name);
		
	errno = ENOSYS;	
	return NULL;
}

struct inode* fs_creat (struct inode* inode, char* name, mode_t mode) {
	if(likely(inode->creat))
		return inode->creat(inode, name, mode);
	
	errno = ENOSYS;		
	return NULL;
}
	
int fs_rename (struct inode* inode, char* newname) {
	if(likely(inode->rename))
		return inode->rename(inode, newname);
	
	errno = ENOSYS;		
	return -1;
}

int fs_unlink (struct inode* inode, char* name) {
	if(likely(inode->unlink))
		return inode->unlink(inode, name);
		
	errno = ENOSYS;	
	return -1;
}

int fs_chown (struct inode* inode, uid_t owner, gid_t group) {
	if(likely(inode->chown))
		return inode->chown(inode, owner, group);
		
	errno = ENOSYS;	
	return -1;
}

void fs_flush(struct inode* inode) {
	if(likely(inode->flush))
		inode->flush(inode);
		
	errno = ENOSYS;	
	return;
}

int fs_ioctl(struct inode* inode, int req, void* buf) {
	if(likely(inode->ioctl))
		return inode->ioctl(inode, req, buf);
	
	errno = ENOSYS;	
	return -1;
}
