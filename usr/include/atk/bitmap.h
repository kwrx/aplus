#ifndef _ATK_BITMAP_H
#define _ATK_BITMAP_H

#include <atk.h>
#include <stdint.h>


struct atk_bitmap_lockdata {
	atk_rect_t region;
	int flags;

	void* data;
};

typedef struct atk_bitmap {
	atk_size_t size;
	uint32_t stride;

	void* buffer;	
	uint8_t lock;

	struct atk_bitmap_lockdata __lockdata;
} atk_bitmap_t;


typedef struct atk_mask {
	void* buffer;

	uint32_t stride;
	atk_size_t size;
} atk_mask_t;



#define ATK_BITMAP_LOCK_RDONLY			0
#define ATK_BITMAP_LOCK_RDWR			1

#endif
