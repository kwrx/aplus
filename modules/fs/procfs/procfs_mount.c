#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <libc.h>

#include "procfs.h"



static int procfs_open(struct inode* inode) {
	if(unlikely(!inode)) {
		errno = EINVAL;
		return -1;
	}


	volatile task_t* tmp;
	for(tmp = task_queue; tmp; tmp = tmp->next) {
		char buf[32];
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "%d", tmp->pid);

		if(vfs_finddir(inode, buf))
			continue;

		inode_t* i = vfs_mknod(inode, buf, S_IFDIR | 0666);
		if(unlikely(!i)) {
			kprintf(ERROR "procfs: error on creating %s\n", buf);
			return E_ERR;
		}

		procfs_add_childs(i, (task_t*) tmp);
	}

	return 0;
}

static int meminfo_open(struct inode* inode) {
	if(unlikely(!inode)) {
		errno = EINVAL;
		return -1;
	}

	if(!inode->userdata)
		inode->userdata = (void*) kmalloc(sizeof(procfs_t), GFP_KERNEL);

	procfs_t* pfs = (procfs_t*) inode->userdata;


	static char buf[BUFSIZ];
	memset(buf, 0, BUFSIZ);

	sprintf(buf, 
		"MemTotal:       %12d kB\n"
		"MemUsed:        %12d kB\n"
		"MemFree:        %12d kB\n"
		"MemAvailable:   %12d kB\n"
		"Slab:           %12d kB\n"
#if CONFIG_CACHE
		"Cache:          %12d kB\n"
#endif
		,

		pmm_state()->total / 1024,
		pmm_state()->used / 1024,
		(pmm_state()->total - pmm_state()->used) / 1024,
		((pmm_state()->total - pmm_state()->used) + (kvm_state()->total - kvm_state()->used)) / 1024,
		kvm_state()->used / 1024
#if CONFIG_CACHE
		, kcache_state()->used / 1024
#endif
	);

	pfs->data = strdup(buf);
	inode->size = (off64_t) strlen(buf);
	
	return 0;
}



int procfs_mount(struct inode* dev, struct inode* dir) {
	(void) dev;


	dir->open = procfs_open;
	dir->close = NULL;
	dir->finddir = NULL;
	dir->mknod = NULL;
	dir->unlink = NULL;
	dir->read = NULL;
	dir->write = NULL;
	dir->ioctl = NULL;
	dir->rename = NULL;


	inode_t* meminfo = vfs_mknod(dir, "meminfo", S_IFREG | 0666);
	if(unlikely(!meminfo)) {
		kprintf(ERROR "procfs: error on creating meminfo\n");
		return E_ERR;
	}

	meminfo->open = meminfo_open;
	meminfo->read = procfs_read;

	inode_t* i = vfs_mknod(dir, "self", S_IFDIR | 0666);
	if(unlikely(!i)) {
		kprintf(ERROR "procfs: error on creating self\n");
		return E_ERR;
	}

	procfs_add_childs(i, NULL);
	return E_OK;
}
