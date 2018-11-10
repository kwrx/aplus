/* ... */


__fastcall
__optimize(fast)
static void console_output_32(struct cc* cc, int pos, uint8_t style, int32_t ch) {
    
    static int vga_colors[16] = {
        0xFF000000,
        0xFF0000FF,
        0xFF00FF00,
        0xFF0080FF,
        0xFFFF0000,
        0xFFFF00FF,
        0xFFFF8000,
        0xFF808080,
        0xFF404040,
        0xFF6666FF,
        0xFF66FF66,
        0xFF66FFFF,
        0xFFFF6666,
        0xFFFF66FF,
        0xFFFFFF00,
        0xFFFFFFFF
    };
    

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
    uint32_t stride = cc->width << 3;
    uint32_t* offset = &((uint32_t*) cc->frame) [(((pos / cc->width)) * (stride << 4)) + ((pos % cc->width) << 3)];
    
        

    int row, p;
    for(row = 0, p = 0; row < 16; row++, p += stride) {

        int cx, b;
        for(cx = 0, b = 8; cx < 8; cx++, b--)
            if(unlikely(g[row] & (1 << b)))
                offset[cx + p] = vga_colors[(style & 0x0F)];
            else
                offset[cx + p] = vga_colors[(style & 0xF0) >> 4];
    }   
}


__fastcall
__optimize(fast)
static void console_scroll_32(struct cc* cc, int pos) {
    memmove(cc->frame, (void*) ((uintptr_t) cc->frame + (((cc->width << 3) << 2) << 4)), (((cc->width << 3) * ((cc->height - 1) << 3)) << 2) << 3);
}
