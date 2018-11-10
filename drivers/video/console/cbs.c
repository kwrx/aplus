#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>
#include <aplus/mm.h>
#include <aplus/sysconfig.h>
#include <libc.h>

#include "console-priv.h"


#define RGB(r, g, b)                \
    (((r << 16) & 0x00FF0000)   |   \
    ((g << 8) & 0x0000FF00)     |   \
    ((b) & 0x000000FF))


#define DECLARE_CONTEXT(cx)                 \
    context_t* cx = (context_t*) user;      \
    if(unlikely(!cx)) {                     \
        errno = EINVAL;                     \
        return 0;                           \
    }


int console_cbs_damage(VTermRect r, void* user) {
    DECLARE_CONTEXT(cx);

    cx->fb.clear (
        cx, 
        r.start_row, r.end_row,
        r.start_col, r.end_col
    );

    VTermScreenCell c;
    VTermPos p;

    int row, col;
    for(row = r.start_row; row < r.end_row; row++) {
        for(col = r.start_col; col < r.end_col; col++) {
            
            p.col = col;
            p.row = row;
            vterm_screen_get_cell(cx->vs, p, &c);

            cx->fb.putc (
                cx,
                row, col,
                c.chars[0],
                RGB(c.fg.red, c.fg.green, c.fg.blue),
                RGB(c.bg.red, c.bg.green, c.bg.blue)
            );

        }
    }

    memcpy(cx->screen.framebuffer, cx->screen.backbuffer, cx->screen.stride * cx->screen.height);
    __halt();

    return 1;
}


int console_cbs_moverect(VTermRect d, VTermRect s, void* user) {
    DECLARE_CONTEXT(cx);

    cx->fb.move (
        cx,
        d.start_row,
        d.end_row,
        d.start_col,
        d.end_col,
        s.start_row,
        s.end_row,
        s.start_col,
        s.end_col
    );

    return 1;
}

int console_cbs_movecursor(VTermPos p, VTermPos op, int visible, void* user) {
    DECLARE_CONTEXT(cx);

    cx->fb.clear (
        cx,
        op.row, (op.row + 1),
        op.col, (op.col + 1)
    );

    VTermScreenCell c;
    vterm_screen_get_cell(cx->vs, op, &c);

    cx->fb.putc (
        cx,
        op.row, op.col,
        c.chars[0],
        RGB(c.fg.red, c.fg.green, c.fg.blue),
        RGB(c.bg.red, c.bg.green, c.bg.blue)
    );

    cx->cursor.row = p.row;
    cx->cursor.col = p.col;
    return 1;
}

int console_cbs_settermprop(VTermProp p, VTermValue* v, void* user) {
    return 1;
}

int console_cbs_bell(void* user) {
    return 1;
}

int console_cbs_resize(int r, int c, void* user) {
    return 1;
}

int console_cbs_sb_pushline(int cl, const VTermScreenCell* c, void* user) {
    return 1;
}

int console_cbs_sb_popline(int cl, VTermScreenCell* c, void* user) {
    return 1;
}


VTermScreenCallbacks cbs = {
    .damage = console_cbs_damage,
    .moverect = console_cbs_moverect,
    .movecursor = console_cbs_movecursor,
    .settermprop = console_cbs_settermprop,
    .bell = console_cbs_bell,
    .resize = console_cbs_resize,
    .sb_pushline = console_cbs_sb_pushline,
    .sb_popline = console_cbs_sb_popline,
};