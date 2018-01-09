#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/virtio.h>
#include <aplus/utils/list.h>
#include <libc.h>



MODULE_NAME("virtio/network");
MODULE_DEPS("virtio");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


int init(void) {
    return E_OK;
}

int dnit(void) {
    return E_OK;
}