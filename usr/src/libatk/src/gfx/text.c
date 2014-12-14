#include <atk.h>
#include <atk/gfx.h>
#include <atk/bitmap.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

#include "../clrconv.h"
#include "../config.h"

extern atk_gfx_t* __gfx;

#if HAVE_FONTS
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))


int atk_gfx_text_size(char* str, int* width, int* height) {
	assert(HAVE_FONTS);

#if HAVE_FONTS

	*width = 0;
	*height = 0;

	int w = 0;

	FT_GlyphSlot slot = ((FT_Face) __gfx->__c.font.face)->glyph;

	while(*str) {
		char ch = *str++;
		FT_Load_Char((FT_Face) __gfx->__c.font.face, ch, FT_LOAD_DEFAULT);

		switch(ch) {
			case '\n':
				w = 0;
			case '\v':
				*height += __gfx->__c.font.size;
				break;
			case '\r':
				w = 0;
				break;
			default:
				w += slot->advance.x >> 6;
		}

		*width = MAX(w, *width);
	}

	*height += __gfx->__c.font.size;
	return 0;
#endif
}

int atk_gfx_text(char* str, int x, int y) {
	assert(HAVE_FONTS);

#if HAVE_FONTS

	
	atk_bitmap_t* b = __gfx->__c.bitmap;

	if(!b) {
		errno = EINVAL;
		return -1;
	}

	float alpha = __gfx->__c.color[ATK_COLOR_A];

	int width = 0;
	int height = 0;

	assert(atk_gfx_text_size(str, &width, &height) == 0);


	atk_rect_t region = { x, y, width, height };
	void* buffer = atk_bitmap_lockbits(b, region, ATK_BITMAP_LOCK_RDWR);
	assert(buffer);

	
	int bpp = __gfx->bpp >> 3;
	int stride = region[ATK_RECT_W] * bpp;
	int cx = 0;
	int cy = 0;


	FT_GlyphSlot slot = ((FT_Face) __gfx->__c.font.face)->glyph;

	

	while(*str) {
		char ch = *str++;
		FT_Load_Char((FT_Face) __gfx->__c.font.face, ch, FT_LOAD_RENDER);

		switch(ch) {
			case '\n':
				cx = 0;
			case '\v':
				cy += __gfx->__c.font.size;
				break;
			case '\r':
				cx = 0;
				break;
			default: {
				int ocx = slot->bitmap_left;
				int ocy = (cy + (__gfx->__c.font.size)) - slot->bitmap_top;

				int px, py;
				for(px = 0; px < slot->bitmap.width; px++) {
					for(py = 0; py < slot->bitmap.rows; py++) {
						__gfx->__c.color[ATK_COLOR_A] = (float) slot->bitmap.buffer[py * slot->bitmap.width + px] / 255.0f;
				
						__atk_gfx_plot((void*) ((uint32_t) buffer + ((cx + px + ocx) * bpp) + ((0 + py + ocy) * stride)), 1);
		
					}
				}

				cx += slot->advance.x >> 6;
			}
		}		
	}

	atk_bitmap_unlockbits(b);

	__gfx->__c.color[ATK_COLOR_A] = alpha;
#endif
}
