#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(15, unlink,
int sys_unlink(const char* name) {
	char namebuf[1024];
	strcpy(namebuf, name);

	char* s = namebuf;
	char* p = NULL;

	inode_t* cino = NULL;



	if(s[0] == '/') {
		cino = current_task->root;
		s++;
	} else
		cino = current_task->cwd;

	KASSERT(cino);

	
	do {
		if((p = strchr(s, '/')))
			*p++ = '\0';
		else
			break;

		cino = vfs_finddir(cino, s);
		if(unlikely(!cino)) {
			errno = ENOENT;
			return -1;
		}

		if(S_ISLNK(cino->mode)) {
			if(cino->link) {
				if(cino->link == cino) {
					errno = ELOOP;
					return -1;
				}

				cino = cino->link;
			}
		}

		s = p;
	} while(s);

	KASSERT(s);
	KASSERT(*s);
	
	if(vfs_unlink(cino, s) != E_OK) {
		errno = ENOENT;
		return -1;
	}
	
	return 0;
});
