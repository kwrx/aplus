#ifndef _ATK_GFX_H
#define _ATK_GFX_H

#include <atk.h>
#include <atk/bitmap.h>
#include <atk/font.h>
#include <stdint.h>

typedef struct atk_gfx {
	uint16_t width;
	uint16_t height;
	uint32_t stride;
	uint32_t bpp;
	
	void* framebuffer;

	void (*__plot) (void*, atk_color_t, int);
	atk_color_t (*__get) (void*);


	struct {
		atk_bitmap_t* bitmap;
		atk_mask_t* mask;
		atk_color_t color;
		atk_font_t font;
	} __c;
} atk_gfx_t;

#endif
