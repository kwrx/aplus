#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <libc.h>

#include "tmpfs.h"

MODULE_NAME("fs/tmpfs");
MODULE_DEPS("");
MODULE_AUTHOR("WareX");
MODULE_LICENSE("GPL");




int init(void) {
	if(vfs_fsys_register("tmpfs", tmpfs_mount) != E_OK)
		return E_ERR;

	return E_OK;
}



int dnit(void) {
	return E_OK;
}
