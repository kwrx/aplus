/* ... */

__fastcall
__optimize(fast)
static void console_output_textmode(struct cc* cc, int pos, uint8_t style, int32_t ch) {
    ((uint16_t*) CONSOLE_VRAM) [pos] = ((style & 0x7F) << 8) | (ch & 0xFF);
}

__fastcall
__optimize(fast)
static void console_scroll_textmode(struct cc* cc, int pos) {
    memmove((void*) CONSOLE_VRAM, (void*) (CONSOLE_VRAM + (cc->width << 1)), (cc->width * (cc->height - 1)) << 1);
}