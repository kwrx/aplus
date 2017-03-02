#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <libc.h>

#include "procfs.h"

int procfs_mount(struct inode* dev, struct inode* dir) {
	(void) dev;


	#define procfs_add_child(r, a, m, b)							\
		inode_t* a = vfs_mknod(r, #a, m | 0666);					\
		if(unlikely(!a)) {											\
			kprintf(ERROR, "procfs: cannot create %s node\n", #a);	\
			return E_ERR;											\
		}															\
		int a##_open (inode_t* inode) {								\
			static char buf[BUFSIZ];								\
			memset(buf, 0, sizeof(buf));							\
			b														\
			if(strlen(buf)) {										\
				if(likely(inode->userdata))							\
				kfree(inode->userdata);								\
																	\
				inode->userdata = strdup(buf);						\
				inode->size = strlen(buf);							\
				inode->position = 0;								\
			}														\
			return 0;												\
		}															\
		a->open = a##_open;											\
		a->read = procfs_read;






	procfs_add_child(dir, filesystems, S_IFREG, {
		fsys_t* tmp;
		for(tmp = fsys_queue; tmp; tmp = tmp->next) {
			strcat(buf, tmp->name);
			strcat(buf, "\n");	
		}
	});
	
	procfs_add_child(dir, meminfo, S_IFREG, {
		sprintf(buf,
			"MemTotal:      %12d kB\n"
			"MemUsed:       %12d kB\n"
			"MemAvailable:  %12d kB\n"
			"Slab:          %12d kB\n",
			pmm_state()->total / 1024,
			pmm_state()->used / 1024,
			(pmm_state()->total - pmm_state()->used) / 1024,
			kvm_state()->used / 1024
		);
	});
	
	
	procfs_add_child(dir, version, S_IFREG, {
		sprintf(buf, "%s %s-%s %s %s %s\n", 
			KERNEL_NAME, 
			KERNEL_VERSION,
			KERNEL_CODENAME,
			KERNEL_DATE,
			KERNEL_TIME,
			KERNEL_PLATFORM);
	});
	
	procfs_add_child(dir, uptime, S_IFREG, {
		sprintf(buf, "%d.%d\n", (int) sys_times(NULL) / CLOCKS_PER_SEC, (int) sys_times(NULL) % CLOCKS_PER_SEC);
	});
	
	
	procfs_add_child(dir, self, S_IFDIR, {});
	procfs_add_child(self, cwd, S_IFLNK, { inode->link = current_task->cwd; });
	procfs_add_child(self, root, S_IFLNK, { inode->link = current_task->root; });
	procfs_add_child(self, exe, S_IFLNK, { inode->link = current_task->exe; });
	
	procfs_add_child(self, cmdline, S_IFREG, {
		if(current_task->argv != NULL) {
			int i;
			for(i = 0; current_task->argv[i]; i++) {
				strcat(buf, current_task->argv[i]);
				strcat(buf, " ");
			}
		}
	});
	
	procfs_add_child(self, environ, S_IFREG, {
		if(current_task->environ != NULL) {
			int i;
			for(i = 0; current_task->environ[i]; i++) {
				strcat(buf, current_task->environ[i]);
				strcat(buf, "\n");
			}
		}
	});
		
	cwd->link = current_task->cwd;
	root->link = current_task->root;
		
		
		

	return E_OK;
}
