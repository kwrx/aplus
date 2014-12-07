#ifndef _ATK_GFX_H
#define _ATK_GFX_H

#include <atk.h>
#include <stdint.h>

typedef struct atk_gfx {
	uint16_t width;
	uint16_t height;
	uint32_t stride;
	uint32_t bpp;
	
	void* framebuffer;

	void (*__plot) (void*, atk_color_t, int);
	atk_color_t (*__get) (void*);
} atk_gfx_t;

#endif
