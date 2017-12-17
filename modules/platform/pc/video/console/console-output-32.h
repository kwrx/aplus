/* ... */


__fastcall
__optimize(fast)
static void console_output_32(struct cc* cc, int pos, uint16_t ch) {
    
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
    
    
    const uint8_t* g = &__font_bitmap__[(ch & 0xFF) << 4];
    uint32_t stride = cc->width << 3;
    uint32_t* offset = &((uint32_t*) cc->frame) [(((pos / cc->width)) * (stride << 4)) + ((pos % cc->width) << 3)];
    
        

    int row, p;
    for(row = 0, p = 0; row < 16; row++, p += stride) {

        int cx, b;
        for(cx = 0, b = 8; cx < 8; cx++, b--)
            if(unlikely(g[row] & (1 << b)))
                offset[cx + p] = vga_colors[(ch & 0x0F00) >> 8];
            else
                offset[cx + p] = vga_colors[(ch & 0x7000) >> 12];
    }   
}


__fastcall
__optimize(fast)
static void console_scroll_32(struct cc* cc, int pos) {
    memmove(cc->frame, (void*) ((uintptr_t) cc->frame + (((cc->width << 3) << 2) << 4)), (((cc->width << 3) * ((cc->height - 1) << 3)) << 2) << 3);
}
