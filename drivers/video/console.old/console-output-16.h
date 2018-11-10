/* ... */


__fastcall
__optimize(fast)
static void console_output_16(struct cc* cc, int pos, uint8_t style, int32_t ch) {
    
    static uint16_t vga_colors[16] = {
        0x0000,
        0x001F,
        0x007F,
        0x047F,
        0xF800,
        0xF81F,
        0xFC00,
        0x8410,
        0x4208,
        0x633F,
        0x67EC,
        0x67FF,
        0xFB2C,
        0xFB3F,
        0xFFE0,
        0xFFFF
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
    uint16_t* offset = &((uint16_t*) cc->frame) [(((pos / cc->width)) * (stride << 4)) + ((pos % cc->width) << 3)];
    
        

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
static void console_scroll_16(struct cc* cc, int pos) {
    memmove(cc->frame, (void*) ((uintptr_t) cc->frame + (((cc->width << 3) << 1) << 4)), (((cc->width << 3) * ((cc->height - 1) << 3)) << 1) << 3);
}
