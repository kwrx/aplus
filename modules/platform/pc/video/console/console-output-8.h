/* ... */


__fastcall
__optimize(fast)
static void console_output_8(struct cc* cc, int pos, uint16_t ch) {
    
    static uint8_t vga_colors[16] = {
        0x00,
        0x03,
        0x1C,
        0x0F,
        0xE0,
        0xE3,
        0xEC,
        0x6D,
        0x25,
        0x67,
        0x7D,
        0x7F,
        0xED,
        0xEF,
        0xFC,
        0xFF
    };
    
    
    const uint8_t* g = &__font_bitmap__[(ch & 0xFF) << 4];
    uint32_t stride = cc->width << 3;
    uint8_t* offset = &((uint8_t*) cc->frame) [(((pos / cc->width)) * (stride << 4)) + ((pos % cc->width) << 3)];
    
        

    int row, p;
    for(row = 0, p = 0; row < 16; row++, p += stride) {

        int cx, b;
        for(cx = 0, b = 8; cx < 8; cx++, b--)
            if(unlikely(g[row] & (1 << b)))
                offset[cx + p] = vga_colors[(ch & 0x0F00) >> 8];
            else
                offset[cx + p] = vga_colors[(ch & 0xF000) >> 12];
    }   
}


__fastcall
__optimize(fast)
static void console_scroll_8(struct cc* cc, int pos) {
    memmove(cc->frame, (void*) ((uintptr_t) cc->frame + ((cc->width << 3) << 4)), ((cc->width << 3) * ((cc->height - 1) << 3)) << 3);
}
