#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <xdev/task.h>
#include <xdev/timer.h>
#include <libc.h>

#include "iso9660.h"


struct inode* iso9660_finddir(struct inode* inode, char* name) {
	if(unlikely(!inode)) {
		errno = EINVAL;
		return NULL;
	}

	if(unlikely(!inode->userdata)) {
		errno = EINVAL;
		return NULL;
	}

	iso9660_t* ctx = (iso9660_t*) inode->userdata;
	KASSERT(ctx);


	iso9660_dir_t* nodes = (iso9660_dir_t*) kmalloc(iso9660_getlsb32(ctx->dir.length), GFP_USER);
	iso9660_dir_t* snodes = nodes;		
	KASSERT(nodes);

	

	ctx->dev->position = iso9660_getlsb32(ctx->dir.lba) * ISO9660_SECTOR_SIZE;
	if(unlikely(vfs_read(ctx->dev, nodes, iso9660_getlsb32(ctx->dir.length)) != iso9660_getlsb32(ctx->dir.length))) {
		kfree(nodes);

		errno = EIO;
		return NULL;
	}


	
	/* Skip dots (".", "..") */
	nodes = (iso9660_dir_t*) ((uintptr_t) nodes + nodes->size);
	nodes = (iso9660_dir_t*) ((uintptr_t) nodes + nodes->size);


	for(
		; 
		nodes->size;
		nodes = (iso9660_dir_t*) ((uintptr_t) nodes + nodes->size)
	) {

		inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_ATOMIC);
		if(unlikely(!child)) {
			errno = ENOMEM;
			return NULL;
		}

		memset(child, 0, sizeof(inode_t));



		child->name = (const char*) kmalloc(nodes->idlen + 1, GFP_USER);
		KASSERT(child->name);
		
		memset((void*) child->name, 0, nodes->idlen + 1);
		strncpy((char*) child->name, nodes->reserved, nodes->idlen);
		
		iso9660_checkname((char*) child->name);


		if(strcmp(child->name, name) != 0) {
			kfree((void*) child->name);
			kfree((void*) child);

			continue;
		}
	

		
		child->ino = vfs_inode();
		child->mode = (nodes->flags & ISO9660_FLAGS_DIRECTORY ? S_IFDIR : S_IFREG) | 0666 & ~current_task->umask;

		child->dev =
		child->rdev =
		child->nlink = 0;

		child->uid = current_task->uid;
		child->gid = current_task->gid;
		child->size = (off64_t) iso9660_getlsb32(nodes->length);


		child->atime = 
		child->ctime = 
		child->mtime = timer_gettime();
	
		child->parent = inode;
		child->link = NULL;

		child->childs = NULL;


		if(nodes->flags & ISO9660_FLAGS_DIRECTORY) {
			child->open = iso9660_open;
			child->close = iso9660_close;
		} else {
			child->read = iso9660_read;
			child->write = NULL;
		}
		
		child->chown = NULL;
		child->chmod = NULL;
		child->ioctl = NULL;
		

		iso9660_t* cctx = (iso9660_t*) kmalloc(sizeof(iso9660_t), GFP_USER);
		KASSERT(cctx);

		memcpy(cctx, ctx, sizeof(iso9660_t));
		memcpy(&cctx->dir, nodes, sizeof(iso9660_dir_t));

		child->userdata = (void*) cctx;



		

		struct inode_childs* cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
		cx->inode = child;
		cx->next = inode->childs;
		inode->childs = cx;

		return child;
	}


	kfree(snodes);

	errno = ENOENT;
	return NULL;
}
