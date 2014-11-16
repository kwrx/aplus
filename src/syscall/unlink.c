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
	strcpy(p, s);

	return p;
}


int sys_unlink(const char* pathname) {
	if(!pathname) {
		errno = EINVAL;
		return -1;
	}


	inode_t* cwd = NULL;

	if(pathname[0] == '/')
		cwd = vfs_root;
	else
	 	cwd = current_task->cwd;
	
	if(!cwd) {
		if(!vfs_root) {
			kprintf("sys_unlink: no root found for cwd.");
		
			errno = ENOENT;
			return -1;
		}
		
		cwd = vfs_root;
	}
	
	
	if(pathname[0] == '/')
		pathname++;
		
	if(pathname[0] == 0) {
		errno = EPERM;	
		return -1;
	}
	
	char* b = dupstr((char*) pathname);
	char* s = b;
	char* p = s;
	
	while(*s) {
		if((p = strchr(s, '/'))) {
			*p++ = 0;
		
			cwd = (inode_t*) fs_finddir(cwd, s);
			if(!cwd) {
				errno = ENOENT;
				return -1;
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

	if(*s == 0) {
		errno = ENOENT;
		return -1;
	}

	int ret = fs_unlink(cwd, s);
	kfree(b);
	
	return ret;
}

SYSCALL(sys_unlink, 15);
