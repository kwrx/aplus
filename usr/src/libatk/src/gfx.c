#include <atk.h>
#include <atk/gfx.h>
#include <atk/bitmap.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include "clrconv.h"


atk_gfx_t* __gfx;

static void __gfx_plot_pixel_32(void* d, atk_color_t color) {
	atk_color_t dc = ARGB_TO_V4F((*(uint32_t*) d));
	atk_color_t df = __alphablend(dc, color, color[ATK_COLOR_A]);

	*(uint32_t*) d = V4F_TO_ARGB(df);
}

static void __gfx_plot_pixel_24(void* d, atk_color_t color) {
	atk_color_t dc = RGB_TO_V4F((*(uint32_t*) d));
	atk_color_t df = __alphablend(dc, color, color[ATK_COLOR_A]);

	*(uint32_t*) d = V4F_TO_RGB(df);
}

static void __gfx_plot_pixel_16(void* d, atk_color_t color) {
	atk_color_t dc = R5G6B5_TO_V4F((*(uint16_t*) d));
	atk_color_t df = __alphablend(dc, color, color[ATK_COLOR_A]);

	*(uint16_t*) d = V4F_TO_R5G6B5(df);
}

static atk_color_t __gfx_read_pixel_32(void* d) {
	atk_color_t dc = ARGB_TO_V4F((*(uint32_t*) d));

	return dc;
}

static atk_color_t __gfx_read_pixel_24(void* d) {
	atk_color_t dc = RGB_TO_V4F((*(uint32_t*) d));

	return dc;
}

static atk_color_t __gfx_read_pixel_16(void* d) {
	atk_color_t dc = R5G6B5_TO_V4F((*(uint16_t*) d));

	return dc;
}



atk_gfx_t* atk_gfx_create(short width, short height, short bpp, void* buffer) {
	if(!(width && height && bpp && buffer)) {
		errno = EINVAL;
		return NULL;
	}

	atk_gfx_t* gfx = (atk_gfx_t*) malloc(width * height * (bpp / 8));
	gfx->width = width;
	gfx->height = height;
	gfx->bpp = bpp;
	gfx->stride = width * (bpp / 8);
	gfx->framebuffer = buffer;

	switch(bpp) {
		case 16:
			gfx->__plot = __gfx_plot_pixel_16;
			gfx->__get = __gfx_read_pixel_16;
			break;
		case 24:
			gfx->__plot = __gfx_plot_pixel_24;
			gfx->__get = __gfx_read_pixel_24;
			break;
		case 32:
			gfx->__plot = __gfx_plot_pixel_32;
			gfx->__get = __gfx_read_pixel_32;
			break;
		default:
			free(gfx);
			errno = EINVAL;
			return NULL;
	}

	return gfx;
}

void atk_gfx_set(atk_gfx_t* gfx) {
	__gfx = gfx;
}

atk_gfx_t* atk_gfx_get() {
	return __gfx;
}



int atk_gfx_line(atk_bitmap_t* b, int x0, int y0, int x1, int y1, atk_color_t color) {

	if(!b) {
		errno = EINVAL;
		return -1;
	}

	if(x0 > x1)
		__SWAP(x0, x1)

	if(y0 > y1)
		__SWAP(y0, y1)

	atk_rect_t region = { x0, y0, x1, y1 };
	void* buffer = atk_bitmap_lockbits(b, region, ATK_BITMAP_LOCK_RDWR);
	if(!buffer)
		return -1;
	
	x1 -= x0;
	y1 -= y1;
	x0 = 0;
	y0 = 0;
	
	int x2 = 0;
	int sx = 0;
	int sy = 0;
	int dx = x1;
	int dy = y1;
	int e = dx - dy;
	int e2 = 0;
	int ed = dx + dy == 0 ? 1 : sqrt((float) (dx * dx) + (float) (dy * dy));
	int bpp = __gfx->bpp >> 3;
	int stride = x1 * bpp;

	for(;;) {
		color[ATK_COLOR_A] = abs(e - dx + dy) / ed;
		__gfx->__plot((void*) ((uint32_t) buffer + (y0 * stride) + (x0 * bpp)), color);
		
		e2 = e;
		x2 = x0;	

		if((e2 << 1) >= -dx) {
			if(x0 == x1)
				break;

			if(e2 + dy < ed) {
				color[ATK_COLOR_A] = abs(e2 - dy) / ed;
				__gfx->__plot((void*) ((uint32_t) buffer + ((y0 + sy) * stride) + (x0 * bpp)), color);		
			}

			e -= dy;
			x0 += sx;
		}

		if((e2 << 1) <= dy) {
			if(y0 == y1)
				break;

			if(dx - e2 < ed) {
				color[ATK_COLOR_A] = abs(dx - e2) / ed;
				__gfx->__plot((void*) ((uint32_t) buffer + (y0 * stride) + ((x2 + sx) * bpp)), color);		
			}

			e += dx;
			y0 += sy;
		}
	}
	
	atk_bitmap_unlockbits(b);
}

