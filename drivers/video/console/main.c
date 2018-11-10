#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/mm.h>
#include <libc.h>

#include "console-priv.h"

MODULE_NAME("video/console2");
MODULE_DEPS("video/fb,pc/video/bga"); /* FIXME */
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static void* __kmalloc(size_t size, void* user) {
    return kmalloc(size, GFP_KERNEL);
}

static void __kfree(void* p, void* user) {
    return kfree(p);
}

VTermAllocatorFunctions mm = {
    .malloc = __kmalloc,
    .free = __kfree
};



int init(void) {
    context_t* cx = kcalloc(1, sizeof(context_t), GFP_KERNEL);
    if(unlikely(!cx)) {
        errno = ENOMEM;
        return -1;
    }

    if(fb_init(cx) != 0)
        return -1;

    
    cx->vt = vterm_new_with_allocator(cx->console.rows, cx->console.cols, &mm, NULL);
    cx->vs = vterm_obtain_screen(cx->vt);

    vterm_set_utf8(cx->vt, 1);
    vterm_screen_set_callbacks(cx->vs, &cbs, cx);
    vterm_screen_reset(cx->vs, 0);

    VTermRect r = {
        .start_row = 0,
        .start_col = 0,
        .end_row = cx->console.rows,
        .end_col = cx->console.cols
    };


    console_cbs_damage(r, cx);
    vterm_input_write(cx->vt, "Hello World\n", 13);
    return 0;
}

int dnit(void) {

}