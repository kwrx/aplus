#include <atk.h>
#include <atk/gfx.h>
#include <atk/bitmap.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include "clrconv.h"

 
extern atk_gfx_t* __gfx;
static atk_mask_t* __mask = NULL;


#define __i(c1, c2, p)		\
	((int)((1.0f - p) * (float)c1 + p * (float)c2))


atk_mask_t* atk_mask_create_solid(int width, int height, atk_color_t color) {
	atk_mask_t* mask = (atk_mask_t*) malloc(sizeof(atk_mask_t));
	mask->buffer = (void*) malloc((width * height) * (__gfx->bpp >> 3));
	mask->stride = width * (__gfx->bpp >> 3);

	mask->size[ATK_SIZE_W] = width;
	mask->size[ATK_SIZE_H] = height;

	for(register int y = 0, s = (int) mask->buffer; y < height; y++, s += mask->stride)
		__gfx->__plot(s, color, width);

	return mask;
}

atk_mask_t* atk_mask_create_linear_hblend(int width, int height, atk_color_t cl1, atk_color_t cl2) {
	atk_mask_t* mask = (atk_mask_t*) malloc(sizeof(atk_mask_t));
	mask->buffer = (void*) malloc((width * height) * (__gfx->bpp >> 3));
	mask->stride = width * (__gfx->bpp >> 3);

	mask->size[ATK_SIZE_W] = width;
	mask->size[ATK_SIZE_H] = height;


	atk_color_t cl = ATK_COLOR_BLACK;

	for(register int y = 0, s = (int) mask->buffer; y < height; y++, s += mask->stride) {
		for(register int x = 0; x < width; x++) {
					
			cl[ATK_COLOR_A] = __i(cl1[ATK_COLOR_A], cl2[ATK_COLOR_A], ((float)x / (float)width));
			cl[ATK_COLOR_R] = __i(cl1[ATK_COLOR_R], cl2[ATK_COLOR_R], ((float)x / (float)width));
			cl[ATK_COLOR_G] = __i(cl1[ATK_COLOR_G], cl2[ATK_COLOR_G], ((float)x / (float)width));
			cl[ATK_COLOR_B] = __i(cl1[ATK_COLOR_B], cl2[ATK_COLOR_B], ((float)x / (float)width));

			__gfx->__plot(s + (x + (__gfx->bpp >> 3)), cl, 1);
		}
	}
	return mask;
}

atk_mask_t* atk_mask_create_linear_vblend(int width, int height, atk_color_t cl1, atk_color_t cl2) {
	atk_mask_t* mask = (atk_mask_t*) malloc(sizeof(atk_mask_t));
	mask->buffer = (void*) malloc((width * height) * (__gfx->bpp >> 3));
	mask->stride = width * (__gfx->bpp >> 3);

	mask->size[ATK_SIZE_W] = width;
	mask->size[ATK_SIZE_H] = height;


	atk_color_t cl = ATK_COLOR_BLACK;

	for(register int y = 0, s = (int) mask->buffer; y < height; y++, s += mask->stride) {
		cl[ATK_COLOR_A] = __i(cl1[ATK_COLOR_A], cl2[ATK_COLOR_A], ((float)y / (float)height));
		cl[ATK_COLOR_R] = __i(cl1[ATK_COLOR_R], cl2[ATK_COLOR_R], ((float)y / (float)height));
		cl[ATK_COLOR_G] = __i(cl1[ATK_COLOR_G], cl2[ATK_COLOR_G], ((float)y / (float)height));
		cl[ATK_COLOR_B] = __i(cl1[ATK_COLOR_B], cl2[ATK_COLOR_B], ((float)y / (float)height));

		__gfx->__plot(s, cl, width);
	}
	return mask;
}

atk_mask_t* atk_mask_create_radial_blend(int width, int height, atk_color_t cl1, atk_color_t cl2) {
	return NULL;
}


int atk_mask_destroy(atk_mask_t* mask) {
	if(__mask == mask)
		__mask = NULL;

	free(mask->buffer);
	free(mask);
	return 0;
}

atk_mask_t* atk_get_mask() {
	return __mask;
}

int atk_set_mask(atk_mask_t* mask) {
	__mask = mask;
}
