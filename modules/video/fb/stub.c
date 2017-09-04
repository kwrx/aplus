#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <libc.h>

#include <aplus/fbdev.h>

extern fbdev_t* fbdev;

int stub_init(void) {
    fbdev->name = "VGA Controller";
    return 0;
}
