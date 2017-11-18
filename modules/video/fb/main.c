#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <libc.h>

#include <aplus/base.h>
#include <aplus/fb.h>

MODULE_NAME("video/fb");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static list(fbdev_t*, fbdevs);


int fbdev_register_device(fbdev_t* fbdev, char* name) {
    if(unlikely(!fbdev || !name)) {
        errno = EINVAL;
        return E_ERR;
    }

    fbdev->name = strdup(name);
    //fbdev->inode->ioctl = fb_ioctl;
    
    int e = E_OK;
    if(likely(fbdev->init))
        e = fbdev->init(fbdev);

    kprintf(INFO "fb: register device %s\n", fbdev->name);
    return E_OK; 
}

int fbdev_unregister_device(fbdev_t* fbdev) {
    if(likely(fbdev->dnit))
        if(unlikely(fbdev->dnit(fbdev) != E_OK))
            kprintf(WARN "fb: (%s)->dnit() error!\n", fbdev->name);

    list_remove(fbdevs, fbdev);
    kprintf(INFO "fb: unregister device %s\n", fbdev->name);  
    return E_OK;
}


int init(void) {
    memset(&fbdevs, 0, sizeof(fbdevs));
    return E_OK;
}


int dnit(void) {
    list_each(fbdevs, v)
        v->dnit(v);

    list_clear(fbdevs);
    return E_OK;
}
