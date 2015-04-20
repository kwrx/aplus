#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

#include <stdio.h>

extern task_t* current_task;
extern inode_t* vfs_root;



static char* dupstr(char* s) {
	char* p = (char*) kmalloc(strlen(s) + 1);
	if(unlikely(!p))
		return NULL;

	strcpy(p, s);
	return p;
}


static inode_t* ino_open(char* filename, int flags, mode_t mode) {

	if(unlikely(!filename)) {
		errno = EINVAL;
		return NULL;
	}

	inode_t* cwd = NULL;
	inode_t* root = NULL;


	if(likely(current_task))
		root = current_task->root;
	else
		root = vfs_root;


	if(filename[0] == '/')
		cwd = root;
	else
	 	cwd = current_task->cwd;
	
	if(unlikely(!cwd)) {
		if(!root) {
			kprintf("sys_open: no root found for cwd.");
		
			errno = ENOENT;
			return NULL;
		}
		
		cwd = root;
	}
	
	
	if(filename[0] == '/')
		filename++;
		
	if(filename[0] == 0)
		return cwd;
		
	
	char* s = filename;
	char* p = s;
	
	while(*s) {
		if((p = strchr(s, '/'))) {
			*p++ = 0;
		
			cwd = (inode_t*) fs_finddir(cwd, s);
			if(unlikely(!cwd)) {
				errno = ENOENT;
				return NULL;
			}

			while(S_ISLNK(cwd->mode))
				if(cwd->link)
					cwd = cwd->link;
				else
					break;

			s = p;
		} else
			break;
	}

	if(unlikely(*s == 0)) {
		errno = ENOENT;
		return NULL;
	}

	inode_t* ent = (inode_t*) fs_finddir(cwd, s);


	if(flags & O_EXCL) {
		if(unlikely(ent)) {
			errno = EEXIST;
			return NULL;
		}
	}


	if(flags & O_CREAT) {
		if(!ent) {
			if(flags & O_VIRT)
				ent = (inode_t*) vfs_mknod(cwd, s, mode);
			else
				ent = (inode_t*) fs_creat(cwd, s, mode);
		}
	}

	if(unlikely(!ent)) {
		errno = ENOENT;
		return NULL;
	}

	if(!(flags & O_NOFOLLOW)) {
		while(S_ISLNK(ent->mode)) {
			if(unlikely(ent == ent->link)) {
				errno = ELOOP;
				return NULL;
			}			

			if(ent->link)
				ent = ent->link;
			else
				break;
		}
	}

	if(flags & O_DIRECTORY) {
		if(!(S_ISDIR(ent->mode))) {
			errno = ENOTDIR;
			return NULL;
		}
	}

	return ent;
}

int sys_open(char* filename, int flags, mode_t mode) {
	if(unlikely(!current_task))
		return -1;

#ifdef IO_DEBUG
	kprintf("io: open/create \"%s\"\n", filename);
#endif

	char* p = dupstr(filename);
	inode_t* ino = ino_open(p, flags, mode);
	kfree(p);

	if(unlikely(!ino))
		return -1;
	
	
	return schedule_append_fd(current_task, ino);
}

SYSCALL(sys_open, 10);
