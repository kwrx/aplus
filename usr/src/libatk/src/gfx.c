#include <atk.h>
#include <atk/gfx.h>
#include <atk/bitmap.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include "clrconv.h"


atk_gfx_t* __gfx;

static void __gfx_plot_pixel_32(void* d, atk_color_t color, int width) {
	if(color[ATK_COLOR_A] == 0.0)
		return;

	if(color[ATK_COLOR_A] == 1.0) {
		uint32_t dd = V4F_TO_ARGB(color);

		for(register int i = 0; i < width; i++)
			((uint32_t*) d)[i] = dd;

		return;
	}

	for(register int i = 0; i < width; i++) {
		atk_color_t dc = ARGB_TO_V4F(((uint32_t*) d)[i]);
		atk_color_t df = __alphablend(dc, color, color[ATK_COLOR_A]);

		((uint32_t*) d)[i] = V4F_TO_ARGB(df);
	}
}

static void __gfx_plot_pixel_24(void* d, atk_color_t color, int width) {
	if(color[ATK_COLOR_A] == 0.0)
		return;

	if(color[ATK_COLOR_A] == 1.0) {
		uint32_t dd = V4F_TO_RGB(color);

		for(register int i = 0; i < width; i++)
			((uint32_t*) d)[i] = dd;

		return;
	}

	for(register int i = 0; i < width; i++) {
		atk_color_t dc = RGB_TO_V4F(((uint32_t*) d)[i]);
		atk_color_t df = __alphablend(dc, color, color[ATK_COLOR_A]);

		((uint32_t*) d)[i] = V4F_TO_RGB(df);
	}
}

static void __gfx_plot_pixel_16(void* d, atk_color_t color, int width) {
	if(color[ATK_COLOR_A] == 0.0)
		return;

	if(color[ATK_COLOR_A] == 1.0) {
		uint16_t dd = V4F_TO_R5G6B5(color);

		for(register int i = 0; i < width; i++)
			((uint16_t*) d)[i] = dd;

		return;
	}

	for(register int i = 0; i < width; i++) {
		atk_color_t dc = R5G6B5_TO_V4F(((uint16_t*) d)[i]);
		atk_color_t df = __alphablend(dc, color, color[ATK_COLOR_A]);

		((uint16_t*) d)[i] = V4F_TO_R5G6B5(df);
	}
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


__attribute__((fastcall))
static void __atk_gfx_plot(void* buf, int w) {
	if(__gfx->__c.mask)
		return;
	else
		__gfx->__plot(buf, __gfx->__c.color, w);
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

void atk_set_gfx(atk_gfx_t* gfx) {
	__gfx = gfx;
}

atk_gfx_t* atk_get_gfx() {
	return __gfx;
}


void atk_gfx_set_color(atk_color_t color) {
	if(!__gfx)
		return;

	__gfx->__c.color = color;
}

void atk_gfx_set_color_argb(float a, float r, float g, float b) {
	if(!__gfx)
		return;

	atk_color_t c = { a, r, g, b };
	__gfx->__c.color = c;
}

void atk_gfx_set_color_rgb(float r, float g, float b) {
	if(!__gfx)
		return;

	atk_color_t c = { 1.0f, r, g, b };
	__gfx->__c.color = c;
}

void atk_gfx_set_bitmap(atk_bitmap_t* bitmap) {
	if(!__gfx)
		return;

	__gfx->__c.bitmap = bitmap;
}



int atk_gfx_clear() {
	atk_bitmap_t* b = __gfx->__c.bitmap;
	atk_color_t color = __gfx->__c.color;

	if(!b) {
		errno = EINVAL;
		return -1;
	}

	color[ATK_COLOR_A] = 1.0f;
	for(int p = (int) b->buffer, i = 0; i < b->size[ATK_SIZE_H]; i++, p += b->stride)
		__atk_gfx_plot((void*) p, b->size[ATK_SIZE_W]);

	return 0;
}


int atk_gfx_hline(int x0, int y0, int x1) {
	atk_bitmap_t* b = __gfx->__c.bitmap;
	atk_color_t color = __gfx->__c.color;

	if(!b) {
		errno = EINVAL;
		return -1;
	}

	if(x0 > x1)
		__SWAP(x0, x1);

	atk_rect_t region = { x0, y0, x1 - x0, 1 };
	void* buffer = atk_bitmap_lockbits(b, region, ATK_BITMAP_LOCK_RDWR);
	if(!buffer)
		return -1;

	__atk_gfx_plot(buffer, x1 - x0);
	
	atk_bitmap_unlockbits(b);
	return 0;
}

int atk_gfx_vline(int x0, int y0, int y1) {
	atk_bitmap_t* b = __gfx->__c.bitmap;
	atk_color_t color = __gfx->__c.color;

	if(!b) {
		errno = EINVAL;
		return -1;
	}

	if(y0 > y1)
		__SWAP(y0, y1);

	atk_rect_t region = { x0, y0, 1, y1 - y0 };
	void* buffer = atk_bitmap_lockbits(b, region, ATK_BITMAP_LOCK_RDWR);
	if(!buffer)
		return -1;

	__atk_gfx_plot(buffer, y1 - y0);
	
	atk_bitmap_unlockbits(b);
	return 0;
}

int atk_gfx_line(int ix0, int iy0, int ix1, int iy1) {
	atk_bitmap_t* b = __gfx->__c.bitmap;
	atk_color_t color = __gfx->__c.color;

	if(!b) {
		errno = EINVAL;
		return -1;
	}

	#define __PLOT(xp, yp, c)															\
		color[ATK_COLOR_A] = c;															\
		__atk_gfx_plot(																	\
			(void*) ((uint32_t) buffer + (uint32_t) ((yp * stride) + (xp * bpp))),		\
			1																			\
		);

	#define __IPART(f)																	\
		((float) ((int) f))

	#define __FPART(f)																	\
		(f < 0.0f ? (1.0f - (f - floor(f))) : (f - floor(f)))

	#define __RFPART(f)																	\
		(1.0f - __FPART(f))
						
	#define __ROUND(f)																	\
		(__IPART((f + 0.5f)))


	if(iy0 == iy1)
		return atk_gfx_hline(ix0, iy0, ix1);

	if(ix0 == ix1)
		return atk_gfx_vline(ix0, iy0, iy1);


	int steep = abs(iy1 - iy0) > abs(ix1 - ix0);
	if(steep) {
		__SWAP(ix0, iy0);
		__SWAP(ix1, iy1);
	}

	if(ix0 > ix1) {
		__SWAP(ix0, ix1);
		__SWAP(iy0, iy1);
	}

	atk_rect_t region = { ix0, iy0, ix1 - ix0, iy1 - iy0 };
	void* buffer = atk_bitmap_lockbits(b, region, ATK_BITMAP_LOCK_RDWR);
	if(!buffer)
		return -1;
	
	int stride = ix1 * (__gfx->bpp >> 3);
	int bpp = __gfx->bpp >> 3;

	float x1 = ix1 - ix0;
	float y1 = iy1 - iy0;
	float x0 = 0;
	float y0 = 0;
	
	float gr = (float)y1 / (float)x1;
	float xe = __ROUND(x0);
	float ye = (float) y0 + gr * (float)(xe - x0);
	float xg = __RFPART(((float)x0 + 0.5f));
	float xpxl1 = xe;
	float ypxl1 = __IPART(ye);

	if(steep) {
		__PLOT(ypxl1, xpxl1, __RFPART(ye) * xg);
		__PLOT(ypxl1 + 1.0f, xpxl1, __FPART(ye) * xg);
	} else {
		__PLOT(xpxl1, ypxl1, __RFPART(ye) * xg);
		__PLOT(xpxl1, ypxl1 + 1.0f, __FPART(ye) * xg);
	}

	float intery = ye + gr;

	xe = __ROUND(x1);
	ye = (float) y1 + gr * (float)(xe - x1);
	xg = __FPART(((float) x1 + 0.5f));
	
	float xpxl2 = xe;
	float ypxl2 = __IPART(ye);

	if(steep) {
		__PLOT(ypxl2, xpxl2, __RFPART(ye) * xg);
		__PLOT(ypxl2 + 1.0f, xpxl2, __FPART(ye) * xg);
	} else {
		__PLOT(xpxl2, ypxl2, __RFPART(ye) * xg);
		__PLOT(xpxl2, ypxl2 + 1.0f, __FPART(ye) * xg);
	}

	if(steep) {
		for(float x = xpxl1 + 1.0f; x < xpxl2; x += 1.0f) {
			__PLOT(__IPART(intery), x, __RFPART(intery));
			__PLOT(__IPART(intery) + 1.0f, x, __FPART(intery));
			intery += gr;
		}
	} else {
		for(float x = xpxl1 + 1.0f; x < xpxl2; x += 1.0f) {
			__PLOT(x, __IPART(intery), __RFPART(intery));
			__PLOT(x, __IPART(intery) + 1.0f, __FPART(intery));
			intery += gr;
		}
	}

	atk_bitmap_unlockbits(b);
	return 0;
}


int atk_gfx_fill_rectangle(int x, int y, int w, int h) {
	atk_bitmap_t* b = __gfx->__c.bitmap;
	atk_color_t color = __gfx->__c.color;

	if(!b) {
		errno = EINVAL;
		return -1;
	}


	atk_rect_t region = { x, y, w, h };
	void* buffer = atk_bitmap_lockbits(b, region, ATK_BITMAP_LOCK_RDWR);
	if(!buffer)
		return -1;

	int stride = w * (__gfx->bpp >> 3);
	for(int p = (int) buffer, i = 0; i < h; i++, p += stride)
		__atk_gfx_plot(p, w);

	atk_bitmap_unlockbits(b);
	return 0;
}

int atk_gfx_stroke_rectangle(int x, int y, int w, int h) {
	atk_bitmap_t* b = __gfx->__c.bitmap;
	atk_color_t color = __gfx->__c.color;

	if(!b) {
		errno = EINVAL;
		return -1;
	}

	atk_gfx_hline(x, y, x + w);
	atk_gfx_hline(x, y + h, x + w);
	atk_gfx_vline(x, y, y + h);
	atk_gfx_vline(x + w, y, y + h);

	return 0;
}

