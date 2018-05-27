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
static uintptr_t fbid;


static int fb_ioctl(struct inode* inode, int req, void* ptr) {
    
    #define cp(x)                   \
        if(unlikely(!x)) {          \
            errno = EINVAL;         \
            return -1;           \
        }

    fbdev_t* fbdev = (fbdev_t*) inode->userdata;
    cp(fbdev);


    switch(req) {
        case FBIOGET_VSCREENINFO:
            cp(ptr);
            
            memcpy(ptr, &fbdev->vs, sizeof(struct fb_var_screeninfo));
            break;

        case FBIOPUT_VSCREENINFO:
            cp(ptr);
            
            memcpy(&fbdev->vs, ptr, sizeof(struct fb_var_screeninfo));

            if(likely(fbdev->update))
                return fbdev->update(fbdev);

            break;

        case FBIOGET_FSCREENINFO:
            cp(ptr);
            
            memcpy(ptr, &fbdev->fs, sizeof(struct fb_fix_screeninfo));
            break;
            
        default:
            errno = ENOSYS;
            return -1;
    }
    
    return 0;
}

int fbdev_register_device(fbdev_t* fbdev, char* name) {
    if(unlikely(!fbdev || !name)) {
        errno = EINVAL;
        return -1;
    }


    if(unlikely((fbdev->inode = vfs_mkdev("fb", fbid++, S_IFCHR | 0222)) == NULL))
        return -1;

    fbdev->name = strdup(name);
    fbdev->inode->ioctl = fb_ioctl;
    fbdev->inode->userdata = (void*) fbdev;
    
    int e = 0;
    if(likely(fbdev->init))
        e = fbdev->init(fbdev);

    kprintf(INFO "fb%d: registered \'%s\'\n", fbid - 1, fbdev->name);
    return 0; 
}

int fbdev_unregister_device(fbdev_t* fbdev) {
    if(likely(fbdev->dnit))
        if(unlikely(fbdev->dnit(fbdev) != 0))
            kprintf(WARN "fb: (%s)->dnit() error!\n", fbdev->name);


    fbdev->inode->ioctl = NULL;
    list_remove(fbdevs, fbdev);

    kprintf(INFO "fb: unregistered \'%s\'\n", fbdev->name);  
    return 0;
}


int init(void) {
    memset(&fbdevs, 0, sizeof(fbdevs));
    fbid = 0;

    return 0;
}


int dnit(void) {
    list_each(fbdevs, v)
        v->dnit(v);

    list_clear(fbdevs);
    return 0;
}
