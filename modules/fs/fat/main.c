#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

MODULE_NAME("fs/fat");
MODULE_DEPS("");
MODULE_AUTHOR("Antonio Natale");
MODULE_LICENSE("GPL");


extern int fat_mount(struct inode*, struct inode*);


int init(void) {
	if(vfs_fsys_register("fat", fat_mount) != E_OK)
		return E_ERR;

	return E_OK;
}



int dnit(void) {
	return E_OK;
}
