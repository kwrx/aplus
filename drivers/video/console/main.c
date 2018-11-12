#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/mm.h>
#include <aplus/kd.h>
#include <libc.h>

#include "console-priv.h"


MODULE_NAME("video/console");
MODULE_DEPS("video/fb,pc/video/bga"); /* FIXME */
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static void* __kmalloc(size_t size, void* user) {
    return kcalloc(1, size, GFP_KERNEL);
}

static void __kfree(void* p, void* user) {
    return kfree(p);
}

VTermAllocatorFunctions mm = {
    .malloc = __kmalloc,
    .free = __kfree
};



static int console_write(struct inode* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!inode || !buf || !inode->userdata)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(!size))
        return 0;


    context_t* cx = (context_t*) inode->userdata;
    vterm_input_write(cx->vt, buf, size);

    return size;
}


static int console_ioctl(struct inode* inode, int req, void* arg) {
    if(unlikely(!inode || !inode->userdata)) {
        errno = EINVAL;
        return -1;
    }


    context_t* cx = (context_t*) inode->userdata;

    switch(req) {
        case KDGETLED:
        case KDSETLED:
        case KDGKBLED:
        case KDSKBLED:
            errno = ENOSYS;
            return -1;
        case KDGKBTYPE:
            return 0x02; /* KB_101 */
        case KDADDIO:
        case KDDELIO:
        case KDENABIO:
            errno = ENOSYS;
            return -1;
        case KDSETMODE:
            switch((int) arg) {
                case KD_TEXT:
                    errno = ENOSYS;
                    return -1;
                case KD_GRAPHICS:
                    if(fb_init(cx) != 0)
                        return -1;
                    
                    cx->vmode = KD_GRAPHICS;
                    return 0;
                default:
                    errno = EINVAL;
                    return -1;
            }
        case KDGETMODE:
            return cx->vmode;
        case KDMKTONE:
        case KIOCSOUND:
            errno = ENOSYS;
            return -1;
    }   
    
    
    errno = EINVAL;
    return -1;
}


int init(void) {
    context_t* cx = kcalloc(1, sizeof(context_t), GFP_KERNEL);
    if(unlikely(!cx)) {
        errno = ENOMEM;
        return -1;
    }


    cx->vmode = KD_TEXT;

    if(fb_init(cx) != 0)
        return -1;

    
    cx->vt = vterm_new_with_allocator(cx->console.rows, cx->console.cols, &mm, NULL);
    cx->vs = vterm_obtain_screen(cx->vt);
    cx->vc = vterm_obtain_state(cx->vt);

    //vterm_set_utf8(cx->vt, 1);
    vterm_screen_set_callbacks(cx->vs, &cbs, cx);
    vterm_screen_reset(cx->vs, 0);
    vterm_state_reset(cx->vc, 0);


    VTermRect r = {
        .start_row = 0,
        .start_col = 0,
        .end_row = cx->console.rows,
        .end_col = cx->console.cols
    };

    console_cbs_damage(r, cx);
    vterm_input_write(cx->vt, "\e[20h", 5);


    inode_t* ino = vfs_mkdev("console", -1, S_IFCHR | 0222);
    ino->ioctl = console_ioctl;
    ino->write = console_write;
    ino->userdata = cx;

    return 0;
}

int dnit(void) {
    return 0;
}