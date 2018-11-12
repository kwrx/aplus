/* ... */


__fastcall
__optimize(fast)
static void console_output_render_24(context_t* cx, uint16_t sr, uint16_t er, uint16_t sc, uint16_t ec) {
    long sy = (sr * ROW);
    long ey = (er * ROW);
    long sx = (sc * COL);
    long ex = (ec * COL);

    for(; sy < ey; sy++)
        memcpy (
            (void*) ((uintptr_t) cx->screen.framebuffer + (sy * cx->screen.stride) + (sx * 3)),
            (void*) ((uintptr_t) cx->screen.backbuffer + (sy * cx->screen.stride) + (sx * 3)),
            (ex - sx) * 3
        );
}


__fastcall
__optimize(fast)
static void console_output_move_24(context_t* cx, uint16_t dsr, uint16_t der, uint16_t dsc, uint16_t dec, uint16_t ssr, uint16_t ser, uint16_t ssc, uint16_t sec) {
    long dx0 = dsc * COL;
    long dx1 = dec * COL;
    long dy0 = dsr * ROW;
    long dy1 = der * ROW;

    long sx0 = ssc * COL;
    long sx1 = sec * COL;
    long sy0 = ssr * ROW;
    long sy1 = ser * ROW;


    for(long y = 0; y < (dy1 - dy0); y++)
        memmove(
            (void*) ((uintptr_t) cx->screen.backbuffer + ((dy0 + y) * cx->screen.stride) + (dx0 * 3)),
            (void*) ((uintptr_t) cx->screen.backbuffer + ((sy0 + y) * cx->screen.stride) + (sx0 * 3)),
            (sx1 - sx0) * 3
        );
}



__fastcall
__optimize(fast)
static void console_output_putc_24(context_t* cx, uint16_t r, uint16_t c, uint32_t ch, uint32_t fg, uint32_t bg) {
    int idx, i;
    for(i = idx = 0; i < __font.Chars; i++) {
        if(unlikely(ch < __font.Index[i]))
            break;

        if(likely(__font.Index[i] != ch))
            continue;

        idx = i;
        break;
    }

    const uint8_t* g = &__font.Bitmap[idx << 4];
    uint32_t* offset = (uint32_t*) &((uint8_t*) cx->screen.backbuffer) [((r * ROW) * cx->screen.stride) + ((c * COL) * 3)];

    int row, p;
    for(row = 0, p = 0; row < 16; row++, p += cx->screen.width) {

        int cx, b;
        for(cx = 0, b = 8; cx < 8; cx++, b--)
            if(unlikely(g[row] & (1 << b)))
                offset[cx + p] = fg; /* FIXME */
            else
                offset[cx + p] = bg; /* FIXME */
    }   
}
