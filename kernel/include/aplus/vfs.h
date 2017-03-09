#ifndef _VFS_H
#define _VFS_H

#include <aplus.h>
#include <libc.h>

#ifndef __ASSEMBLY__



typedef int64_t off64_t;
typedef int64_t ino64_t;

typedef struct inode inode_t;


typedef struct fsys {
	const char* name;
	int (*mount) (struct inode*, struct inode*);

	struct fsys* next;
} fsys_t;

struct inode_childs {
	inode_t* inode;
	struct inode_childs* next;
};

struct inode {
	const char* name;
	dev_t dev;
	ino64_t ino;
	mode_t mode;
	nlink_t nlink;
	uid_t uid;
	gid_t gid;
	dev_t rdev;
	off64_t size;
	off64_t position;

	time_t atime;
	time_t mtime;
	time_t ctime;
	
	void* userdata;

	int (*open) (struct inode* inode);
	int (*close) (struct inode* inode);

	int (*read) (struct inode* inode, void* ptr, size_t len);
	int (*write) (struct inode* inode, void* ptr, size_t len);
	
	
	struct inode* (*finddir) (struct inode* inode, char* name);
	struct inode* (*mknod) (struct inode* inode, char* name, mode_t mode);
	int (*unlink) (struct inode* inode, char* name);
	
	int (*rename) (struct inode* inode, char* newname);
	int (*chown) (struct inode* inode, uid_t owner, gid_t group);
	int (*chmod) (struct inode* inode, mode_t mode);
	
	int (*ioctl) (struct inode* inode, int req, void* buf);


	struct inode_childs* childs;

	struct inode* parent;
	struct inode* link;
};


extern fsys_t* fsys_queue;
extern inode_t* devfs;
extern inode_t* sysfs;


int vfs_init(void);
ino64_t vfs_inode();

int vfs_open(struct inode* inode);
int vfs_close(struct inode* inode);
int vfs_read(struct inode* inode, void* ptr, size_t len);
int vfs_write(struct inode* inode, void* ptr, size_t len);
struct inode* vfs_finddir(struct inode* inode, char* name);
struct inode* vfs_mknod(struct inode* inode, char* name, mode_t mode);
int vfs_unlink(struct inode* inode, char* name);
int vfs_rename(struct inode* inode, char* newname);
int vfs_chown(struct inode* inode, uid_t owner, gid_t group);
int vfs_chmod(struct inode* inode, mode_t mode);
int vfs_ioctl(struct inode* inode, int req, void* buf);

int vfs_mount(struct inode* dev, struct inode* dir, const char* fstype);
int vfs_umount(struct inode* dir);

fsys_t* vfs_fsys_find(const char* name);
int vfs_fsys_register(const char* name, int (*mount) (struct inode*, struct inode*));

inode_t* vfs_mkdev(const char* name, dev_t rdev, mode_t mode);





#endif

#endif
