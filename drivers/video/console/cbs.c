#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
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


    VTermScreenCell c;
    VTermPos p;

    int row, col;
    for(row = r.start_row; row < r.end_row; row++) {
        for(col = r.start_col; col < r.end_col; col++) {
            
            p.col = col;
            p.row = row;
            vterm_screen_get_cell(cx->vs, p, &c);

            if(c.chars[0] < 32)
                c.chars[0] = ' ';

            cx->fb.putc (
                cx,
                row, col,
                c.chars[0],
                RGB(c.fg.red, c.fg.green, c.fg.blue),
                RGB(c.bg.red, c.bg.green, c.bg.blue)
            );

        }
    }


    cx->fb.render (
        cx, 
        r.start_row, r.end_row,
        r.start_col, r.end_col
    );

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
    DECLARE_CONTEXT(cx);
    return 0;
}

int console_cbs_bell(void* user) {
    DECLARE_CONTEXT(cx);
    return 0;
}

int console_cbs_resize(int r, int c, void* user) {
    DECLARE_CONTEXT(cx);
    return 0;
}

int console_cbs_sb_pushline(int cl, const VTermScreenCell* c, void* user) {
    DECLARE_CONTEXT(cx);
    return 0;
}

int console_cbs_sb_popline(int cl, VTermScreenCell* c, void* user) {
    DECLARE_CONTEXT(cx); 
    return 0;
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