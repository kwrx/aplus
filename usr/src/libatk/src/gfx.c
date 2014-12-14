#include <atk.h>
#include <atk/gfx.h>
#include <atk/bitmap.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "clrconv.h"
#include "config.h"


#if HAVE_FONTS
#include <ft2build.h>
#include FT_FREETYPE_H
#endif


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

void atk_gfx_set_font(char* filename) {
	assert(HAVE_FONTS);

#if HAVE_FONTS
	if(__gfx->__c.font.data) {
		FT_Done_Face((FT_Face)__gfx->__c.font.face);
		FT_Done_FreeType((FT_Library) __gfx->__c.font.library);

		free(__gfx->__c.font.data);
		__gfx->__c.font.data = NULL;
	}

	int fd = open(filename, O_RDONLY, 0644);
	assert(fd > 0);

	lseek(fd, 0, SEEK_END);
	int size = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);

	
	__gfx->__c.font.data = (void*) malloc(size);	
	read(fd, __gfx->__c.font.data, size);
	close(fd);

	FT_Init_FreeType(&__gfx->__c.font.library);
	FT_New_Memory_Face((FT_Library) __gfx->__c.font.library, __gfx->__c.font.data, size, 0, &__gfx->__c.font.face);
	
	atk_gfx_set_font_size(12, 72);
#endif
}

void atk_gfx_set_font_size(int size, int dpi) {
	assert(HAVE_FONTS);

#if HAVE_FONTS
	__gfx->__c.font.size = size;
	__gfx->__c.font.dpi = dpi;

	if(__gfx->__c.font.data)
		FT_Set_Char_Size((FT_Face) __gfx->__c.font.face, __gfx->__c.font.size * 64, 0, __gfx->__c.font.dpi, 0);
#endif
}








