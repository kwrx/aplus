/* ... */



__fastcall
__optimize(fast)
static void console_output_24(struct cc* cc, int pos, uint8_t style, int32_t ch) {
    
    static int vga_colors[16] = {
        0x000000,
        0x0000FF,
        0x00FF00,
        0x0080FF,
        0xFF0000,
        0xFF00FF,
        0xFF8000,
        0x808080,
        0x404040,
        0x6666FF,
        0x66FF66,
        0x66FFFF,
        0xFF6666,
        0xFF66FF,
        0xFFFF00,
        0xFFFFFF
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
    uint32_t stride = (cc->width << 3);
    uint32_t stride_3 = (stride) * 3;
    uint8_t* offset = &((uint8_t*) cc->frame) [((((pos / cc->width)) * (stride << 4)) + ((pos % cc->width) << 3)) * 3];
    
        

    int row, p;
    for(row = 0, p = 0; row < 16; row++, p += stride_3) {

        int cx, b;
        for(cx = 0, b = 8; cx < 24; cx += 3, b--) {
            *(uint32_t*) &offset[cx + p] &= 0x000000FF;

            if(unlikely(g[row] & (1 << b)))
                *(uint32_t*) &offset[cx + p] |= (vga_colors[(style & 0x0F)] & 0xFFFFFF) << 8;
            else
                *(uint32_t*) &offset[cx + p] |= (vga_colors[(style & 0xF0) >> 4] & 0xFFFFFF) << 8;
        }
    }   
}


__fastcall
__optimize(fast)
static void console_scroll_24(struct cc* cc, int pos) {
    memmove(cc->frame, (void*) ((uintptr_t) cc->frame + (((cc->width << 3) * 3) << 4)), (((cc->width << 3) * ((cc->height - 1) << 3)) * 3) << 3);
}
