#include <atk.h>
#include <atk/gfx.h>
#include <atk/bitmap.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

#include "../clrconv.h"
#include "../config.h"

extern atk_gfx_t* __gfx;


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




