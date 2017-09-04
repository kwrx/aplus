#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <libc.h>

MODULE_NAME("arch/x86");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");




int init(void) {
#if defined(__i386__) || defined(__x86_64__)
    return E_OK;
#else
    return E_ERR;
#endif
}



int dnit(void) {
    return E_OK;
}
