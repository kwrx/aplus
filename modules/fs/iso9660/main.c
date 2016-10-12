#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <libc.h>

#include "iso9660.h"

MODULE_NAME("fs/iso9660");
MODULE_DEPS("");
MODULE_AUTHOR("Antonio Natale");
MODULE_LICENSE("GPL");




int init(void) {
	if(vfs_fsys_register("iso9660", iso9660_mount) != E_OK)
		return E_ERR;

	return E_OK;
}



int dnit(void) {
	return E_OK;
}
