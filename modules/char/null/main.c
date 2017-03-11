#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <libc.h>

MODULE_NAME("char/null");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


int init(void) {
	if(unlikely(vfs_mkdev("null", -1, S_IFCHR | 0666) == NULL))
		return E_ERR;

	return E_OK;
}



int dnit(void) {
	return E_OK;
}
