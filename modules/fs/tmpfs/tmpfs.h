#ifndef _TMPFS_H
#define _TMPFS_H


int tmpfs_mount(struct inode* dev, struct inode* dir);

struct inode* tmpfs_mknod(struct inode* inode, char* name, mode_t mode);
int tmpfs_unlink(struct inode* inode, char* name);

int tmpfs_write(struct inode* inode, void* ptr, off_t pos, size_t len);
int tmpfs_read(struct inode* inode, void* ptr, off_t pos, size_t len);

#endif
