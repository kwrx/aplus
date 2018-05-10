/* ... */


__fastcall
__optimize(fast)
static void console_output_8(struct cc* cc, int pos, uint8_t style, int32_t ch) {
    
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
    uint8_t* offset = &((uint8_t*) cc->frame) [(((pos / cc->width)) * (stride << 4)) + ((pos % cc->width) << 3)];
    
        

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
static void console_scroll_8(struct cc* cc, int pos) {
    memmove(cc->frame, (void*) ((uintptr_t) cc->frame + ((cc->width << 3) << 4)), ((cc->width << 3) * ((cc->height - 1) << 3)) << 3);
}
