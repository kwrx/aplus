#include <atk.h>
#include <atk/bitmap.h>
#include <atk/gfx.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "clrconv.h"

extern atk_gfx_t* __gfx;

atk_bitmap_t* atk_bitmap_create_for_data(uint16_t width, uint16_t height, void* buffer) {
	if(!(width && height && buffer)) {
		errno = EINVAL;
		return NULL;
	}

	atk_bitmap_t* b = (atk_bitmap_t*) malloc(sizeof(atk_bitmap_t));
	if(!b) {
		errno = ENOMEM;
		return NULL;
	}

	b->size[ATK_SIZE_W] = width;
	b->size[ATK_SIZE_H] = height;

	b->stride = width * (__gfx->bpp / 8);
	b->lock = 0;
	b->buffer = buffer;

	return b;
}

atk_bitmap_t* atk_bitmap_create(uint16_t width, uint16_t height) {
	if(!(width && height)) {
		errno = EINVAL;
		return NULL;
	}

	void* data = (void*) malloc(width * height * (__gfx->bpp >> 3));
	if(!data) {
		errno = ENOMEM;
		return NULL;
	}

	return atk_bitmap_create_for_data(width, height, data);
}


atk_bitmap_t* atk_bitmap_from_framebuffer() {
	static atk_bitmap_t b;
	memset(&b, 0, sizeof(atk_bitmap_t));

	b.size[ATK_SIZE_W] = __gfx->width;
	b.size[ATK_SIZE_H] = __gfx->height;
	b.stride = __gfx->width * (__gfx->bpp / 8);
	b.lock = 0;
	b.buffer = __gfx->framebuffer;

	return &b;
}

int atk_destroy_bitmap(atk_bitmap_t* b) {
	if(!b) {
		errno = EINVAL;
		return -1;
	}

	if(b->lock) {
		errno = EBUSY;
		return -1;
	}

	free(b->buffer);
	free(b);

	return 0;	
}


void* atk_bitmap_lockbits(atk_bitmap_t* b, atk_rect_t rectangle, int flags) {
	if(!b) {
		errno = EINVAL;
		return NULL;
	}

	if(b->lock) {
		errno = EBUSY;
		return NULL;
	}

	b->lock = 1;

	int stride = rectangle[ATK_RECT_W] * (__gfx->bpp >> 3);
	int size = stride * rectangle[ATK_RECT_H];

	void* data = (void*) malloc(size);
	
	for(register int i = 0,
					 s = 0, 
					 p = (b->stride * rectangle[ATK_RECT_Y]) + (rectangle[ATK_RECT_X] * (__gfx->bpp >> 3));
			i < rectangle[ATK_RECT_H];
			i++, s += stride, p += b->stride
		)
		memcpy(
				(void*) ((uint32_t) data + s), 
				(void*) ((uint32_t) b->buffer + p),
				stride
			);


	b->__lockdata.region = rectangle;
	b->__lockdata.data = data;
	b->__lockdata.flags = flags;

	return data;
}

void atk_bitmap_unlockbits(atk_bitmap_t* b) {
	if(!b) {
		errno = EINVAL;
		return NULL;
	}

	
	if(b->__lockdata.flags & ATK_BITMAP_LOCK_RDWR) {

		int stride = b->__lockdata.region[ATK_RECT_W] * (__gfx->bpp >> 3);

		for( int i = 0,
						 s = 0, 
						 p = (b->stride * b->__lockdata.region[ATK_RECT_Y]) + (b->__lockdata.region[ATK_RECT_X] * (__gfx->bpp >> 3));
				i < b->__lockdata.region[ATK_RECT_H];
				i++, s += stride, p += b->stride
			)
			memcpy(
					(void*) ((uint32_t) b->buffer + p),
					(void*) ((uint32_t) b->__lockdata.data + s),
					stride
				);
	}

	b->__lockdata.region[0] = b->__lockdata.region[1] = b->__lockdata.region[2] = b->__lockdata.region[3] = 0;
	b->__lockdata.flags = 0;

	free(b->__lockdata.data);
	b->__lockdata.data = 0;

	b->lock = 0;
}
