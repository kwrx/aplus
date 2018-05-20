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
    return 0;
#else
    return -1;
#endif
}



int dnit(void) {
    return 0;
}
