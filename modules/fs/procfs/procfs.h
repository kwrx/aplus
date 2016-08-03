#ifndef _PROCFS_H
#define _PROCFS_H


int procfs_mount(struct inode* dev, struct inode* dir);
int procfs_read(struct inode* inode, void* ptr, size_t len);

#endif
